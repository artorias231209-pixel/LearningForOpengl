#include "VulkanViewport.h"

#include <QFile>
#include <QLabel>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QMutex>
#include <QMutexLocker>
#include <QVector4D>
#include <QVulkanFunctions>
#include <QVulkanInstance>
#include <QVulkanWindow>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <functional>
#include <limits>
#include <vector>

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegToRad = kPi / 180.0f;
constexpr float kPickEpsilon = 1e-5f;

float Clamp(float value, float minValue, float maxValue) {
  return std::max(minValue, std::min(maxValue, value));
}

bool IntersectRayTriangle(const QVector3D& origin, const QVector3D& direction,
                          const QVector3D& a, const QVector3D& b,
                          const QVector3D& c, float& t) {
  const QVector3D edge1 = b - a;
  const QVector3D edge2 = c - a;
  const QVector3D pvec = QVector3D::crossProduct(direction, edge2);
  const float determinant = QVector3D::dotProduct(edge1, pvec);
  if (std::abs(determinant) < kPickEpsilon) {
    return false;
  }
  const float invDeterminant = 1.0f / determinant;
  const QVector3D tvec = origin - a;
  const float u = QVector3D::dotProduct(tvec, pvec) * invDeterminant;
  if (u < 0.0f || u > 1.0f) {
    return false;
  }
  const QVector3D qvec = QVector3D::crossProduct(tvec, edge1);
  const float v = QVector3D::dotProduct(direction, qvec) * invDeterminant;
  if (v < 0.0f || u + v > 1.0f) {
    return false;
  }
  t = QVector3D::dotProduct(edge2, qvec) * invDeterminant;
  return t > kPickEpsilon;
}

QVector3D ToVector(const Data::Transform& transform) {
  return QVector3D(static_cast<float>(transform.x),
                   static_cast<float>(transform.y),
                   static_cast<float>(transform.z));
}

struct RenderItem {
  uint64_t id = 0;
  Data::RenderCore mesh;
  QVector3D offset;
};

struct RenderScene {
  std::vector<RenderItem> objects;
  uint64_t selectedObjectId = 0;
  int selectedFace = -1;
  Gui::CameraState camera;
  uint64_t revision = 0;
};

struct GpuBuffer {
  VkBuffer buffer = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkDeviceSize size = 0;
};

struct GpuMesh {
  uint64_t id = 0;
  GpuBuffer vertices;
  GpuBuffer indices;
  uint32_t indexCount = 0;
  std::vector<Data::FaceRange> faceRanges;
  QVector3D offset;
};

struct GlobalUniform {
  QMatrix4x4 viewProjection;
};

struct PushConstants {
  QMatrix4x4 model;
  float color[4];
};

}  // namespace

namespace Gui {

class VulkanSceneRenderer;

class VulkanViewport::Window final : public QVulkanWindow {
 public:
  Window() { setFlags(QVulkanWindow::PersistentResources); }

  QVulkanWindowRenderer* createRenderer() override;

  void setRenderScene(const Data::SceneDocument* document, uint64_t selectedObjectId,
                      int selectedFace, const CameraState& camera) {
    RenderScene replacement;
    replacement.selectedObjectId = selectedObjectId;
    replacement.selectedFace = selectedFace;
    replacement.camera = camera;
    if (document) {
      replacement.objects.reserve(document->objects().size());
      for (const Data::SceneObject& object : document->objects()) {
        replacement.objects.push_back(
            {object.state.id, object.mesh, ToVector(object.state.transform)});
      }
    }
    {
      QMutexLocker lock(&m_sceneMutex);
      replacement.revision = m_scene.revision + 1;
      m_scene = std::move(replacement);
    }
    requestUpdate();
  }

  void setCamera(const CameraState& camera) {
    {
      QMutexLocker lock(&m_sceneMutex);
      m_scene.camera = camera;
      ++m_scene.revision;
    }
    requestUpdate();
  }

  CameraState camera() const {
    QMutexLocker lock(&m_sceneMutex);
    return m_scene.camera;
  }

  void setInteractionMode(RenderWidget::InteractionMode mode) { m_mode = mode; }

  RenderScene sceneSnapshot() const {
    QMutexLocker lock(&m_sceneMutex);
    return m_scene;
  }

  std::function<void(uint64_t, int)> selectionCallback;
  std::function<void(uint64_t, const QVector3D&)> moveStartedCallback;
  std::function<void(uint64_t, const QVector3D&)> movePreviewCallback;
  std::function<void(uint64_t, const QVector3D&, const QVector3D&)>
      moveFinishedCallback;
  std::function<void(const CameraState&)> cameraChangedCallback;

 protected:
  void mousePressEvent(QMouseEvent* event) override {
    m_lastMousePos = event->pos();
    if (event->button() == Qt::RightButton) {
      m_orbiting = true;
      return;
    }
    if (event->button() != Qt::LeftButton) {
      return;
    }

    const PickResult pick = pickAt(event->pos());
    if (m_mode == RenderWidget::InteractionMode::SelectBody) {
      notifySelection(pick.hit ? pick.objectId : 0, -1);
      return;
    }
    if (m_mode == RenderWidget::InteractionMode::SelectFace) {
      notifySelection(pick.hit ? pick.objectId : 0, pick.hit ? pick.faceIndex : -1);
      return;
    }
    if (!pick.hit) {
      notifySelection(0, -1);
      return;
    }

    notifySelection(pick.objectId, -1);
    const RenderScene scene = sceneSnapshot();
    const auto selected = std::find_if(scene.objects.begin(), scene.objects.end(),
                                       [&pick](const RenderItem& item) {
                                         return item.id == pick.objectId;
                                       });
    if (selected == scene.objects.end()) {
      return;
    }
    m_dragPlanePoint = objectCenter(*selected);
    m_dragPlaneNormal = cameraForward(scene.camera);
    if (intersectMovePlane(event->pos(), scene.camera, m_dragPlanePoint,
                           m_dragPlaneNormal, m_dragStartWorld)) {
      m_moving = true;
      m_movingObjectId = pick.objectId;
      m_dragStartOffset = selected->offset;
      if (moveStartedCallback) {
        moveStartedCallback(pick.objectId, m_dragStartOffset);
      }
    }
  }

  void mouseMoveEvent(QMouseEvent* event) override {
    const QPoint delta = event->pos() - m_lastMousePos;
    m_lastMousePos = event->pos();
    if (m_orbiting) {
      RenderScene scene = sceneSnapshot();
      scene.camera.yawDeg += static_cast<float>(delta.x()) * 0.6f;
      scene.camera.pitchDeg = Clamp(
          scene.camera.pitchDeg + static_cast<float>(delta.y()) * 0.4f, -85.0f,
          85.0f);
      setCamera(scene.camera);
      if (cameraChangedCallback) {
        cameraChangedCallback(scene.camera);
      }
      return;
    }
    if (!m_moving || m_movingObjectId == 0) {
      return;
    }
    const RenderScene scene = sceneSnapshot();
    QVector3D worldPoint;
    if (intersectMovePlane(event->pos(), scene.camera, m_dragPlanePoint,
                           m_dragPlaneNormal, worldPoint) && movePreviewCallback) {
      movePreviewCallback(m_movingObjectId,
                          m_dragStartOffset + worldPoint - m_dragStartWorld);
    }
  }

  void mouseReleaseEvent(QMouseEvent* event) override {
    if (event->button() == Qt::RightButton) {
      m_orbiting = false;
    }
    if (event->button() != Qt::LeftButton || !m_moving) {
      return;
    }
    const RenderScene scene = sceneSnapshot();
    const auto moved = std::find_if(scene.objects.begin(), scene.objects.end(),
                                    [this](const RenderItem& item) {
                                      return item.id == m_movingObjectId;
                                    });
    if (moved != scene.objects.end() && moveFinishedCallback) {
      moveFinishedCallback(m_movingObjectId, m_dragStartOffset, moved->offset);
    }
    m_moving = false;
    m_movingObjectId = 0;
  }

  void wheelEvent(QWheelEvent* event) override {
    const QPoint degrees = event->angleDelta() / 8;
    if (degrees.y() == 0) {
      return;
    }
    RenderScene scene = sceneSnapshot();
    scene.camera.distance = Clamp(
        scene.camera.distance * std::pow(0.88f, static_cast<float>(degrees.y()) / 15.0f),
        0.6f, 100000.0f);
    setCamera(scene.camera);
    if (cameraChangedCallback) {
      cameraChangedCallback(scene.camera);
    }
  }

 private:
  struct PickResult {
    bool hit = false;
    uint64_t objectId = 0;
    int faceIndex = -1;
  };

  static QVector3D cameraPosition(const CameraState& camera) {
    const float yaw = camera.yawDeg * kDegToRad;
    const float pitch = camera.pitchDeg * kDegToRad;
    const float cp = std::cos(pitch);
    return camera.target + QVector3D(camera.distance * cp * std::cos(yaw),
                                     camera.distance * std::sin(pitch),
                                     camera.distance * cp * std::sin(yaw));
  }

  static QVector3D cameraForward(const CameraState& camera) {
    return (camera.target - cameraPosition(camera)).normalized();
  }

  QMatrix4x4 viewProjection(const CameraState& camera) const {
    QMatrix4x4 projection;
    projection.perspective(45.0f,
                           static_cast<float>(std::max(width(), 1)) /
                               static_cast<float>(std::max(height(), 1)),
                           0.05f, 100000.0f);
    QMatrix4x4 view;
    view.lookAt(cameraPosition(camera), camera.target, QVector3D(0, 1, 0));
    return projection * view;
  }

  bool screenRay(const QPoint& pos, const CameraState& camera, QVector3D& origin,
                 QVector3D& direction) const {
    if (width() <= 0 || height() <= 0) {
      return false;
    }
    const float x = 2.0f * static_cast<float>(pos.x()) / width() - 1.0f;
    const float y = 1.0f - 2.0f * static_cast<float>(pos.y()) / height();
    const QMatrix4x4 inverse = viewProjection(camera).inverted();
    const QVector4D nearPoint = inverse * QVector4D(x, y, -1.0f, 1.0f);
    const QVector4D farPoint = inverse * QVector4D(x, y, 1.0f, 1.0f);
    if (std::abs(nearPoint.w()) < kPickEpsilon ||
        std::abs(farPoint.w()) < kPickEpsilon) {
      return false;
    }
    origin = nearPoint.toVector3DAffine();
    direction = (farPoint.toVector3DAffine() - origin).normalized();
    return direction.lengthSquared() > kPickEpsilon;
  }

  PickResult pickAt(const QPoint& pos) const {
    PickResult best;
    const RenderScene scene = sceneSnapshot();
    QVector3D origin;
    QVector3D direction;
    if (!screenRay(pos, scene.camera, origin, direction)) {
      return best;
    }
    float bestDistance = std::numeric_limits<float>::max();
    for (const RenderItem& item : scene.objects) {
      for (size_t faceIndex = 0; faceIndex < item.mesh.m_faceRanges.size();
           ++faceIndex) {
        const Data::FaceRange& face = item.mesh.m_faceRanges[faceIndex];
        const uint32_t end = face.firstIndex + face.indexCount;
        for (uint32_t i = face.firstIndex; i + 2 < end; i += 3) {
          const Data::Vertex& a = item.mesh.m_vertices[item.mesh.m_indices[i]];
          const Data::Vertex& b = item.mesh.m_vertices[item.mesh.m_indices[i + 1]];
          const Data::Vertex& c = item.mesh.m_vertices[item.mesh.m_indices[i + 2]];
          float distance = 0.0f;
          if (IntersectRayTriangle(origin, direction,
                                   QVector3D(a.x, a.y, a.z) + item.offset,
                                   QVector3D(b.x, b.y, b.z) + item.offset,
                                   QVector3D(c.x, c.y, c.z) + item.offset,
                                   distance) &&
              distance < bestDistance) {
            bestDistance = distance;
            best.hit = true;
            best.objectId = item.id;
            best.faceIndex = static_cast<int>(faceIndex);
          }
        }
      }
    }
    return best;
  }

  bool intersectMovePlane(const QPoint& pos, const CameraState& camera,
                          const QVector3D& planePoint,
                          const QVector3D& planeNormal,
                          QVector3D& intersection) const {
    QVector3D origin;
    QVector3D direction;
    if (!screenRay(pos, camera, origin, direction)) {
      return false;
    }
    const float denominator = QVector3D::dotProduct(direction, planeNormal);
    if (std::abs(denominator) < kPickEpsilon) {
      return false;
    }
    const float distance = QVector3D::dotProduct(planePoint - origin, planeNormal) /
                           denominator;
    if (distance < 0.0f) {
      return false;
    }
    intersection = origin + direction * distance;
    return true;
  }

  static QVector3D objectCenter(const RenderItem& item) {
    const std::array<float, 3> center = item.mesh.center();
    return QVector3D(center[0], center[1], center[2]) + item.offset;
  }

  void notifySelection(uint64_t objectId, int faceIndex) {
    if (selectionCallback) {
      selectionCallback(objectId, faceIndex);
    }
  }

  mutable QMutex m_sceneMutex;
  RenderScene m_scene;
  RenderWidget::InteractionMode m_mode = RenderWidget::InteractionMode::SelectBody;
  bool m_orbiting = false;
  bool m_moving = false;
  uint64_t m_movingObjectId = 0;
  QPoint m_lastMousePos;
  QVector3D m_dragPlanePoint;
  QVector3D m_dragPlaneNormal;
  QVector3D m_dragStartWorld;
  QVector3D m_dragStartOffset;
};

class VulkanSceneRenderer final : public QVulkanWindowRenderer {
 public:
  explicit VulkanSceneRenderer(VulkanViewport::Window* window) : m_window(window) {}

  void initResources() override {
    m_functions = m_window->vulkanInstance()->deviceFunctions(m_window->device());
    createUniformResources();
    createDescriptorResources();
    createPipelines();
  }

  void releaseResources() override {
    if (!m_functions) {
      return;
    }
    const VkDevice device = m_window->device();
    destroyMeshes();
    destroyBuffer(m_axisVertices);
    if (m_meshPipeline) m_functions->vkDestroyPipeline(device, m_meshPipeline, nullptr);
    if (m_facePipeline) m_functions->vkDestroyPipeline(device, m_facePipeline, nullptr);
    if (m_axisPipeline) m_functions->vkDestroyPipeline(device, m_axisPipeline, nullptr);
    if (m_pipelineLayout) m_functions->vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
    if (m_pipelineCache) m_functions->vkDestroyPipelineCache(device, m_pipelineCache, nullptr);
    if (m_descriptorLayout)
      m_functions->vkDestroyDescriptorSetLayout(device, m_descriptorLayout, nullptr);
    if (m_descriptorPool) m_functions->vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
    destroyBuffer(m_uniformBuffer);
    m_meshPipeline = VK_NULL_HANDLE;
    m_facePipeline = VK_NULL_HANDLE;
    m_axisPipeline = VK_NULL_HANDLE;
    m_pipelineLayout = VK_NULL_HANDLE;
    m_pipelineCache = VK_NULL_HANDLE;
    m_descriptorLayout = VK_NULL_HANDLE;
    m_descriptorPool = VK_NULL_HANDLE;
    m_functions = nullptr;
  }

  void startNextFrame() override {
    const RenderScene scene = m_window->sceneSnapshot();
    if (scene.revision != m_uploadedSceneRevision) {
      rebuildMeshes(scene);
      m_uploadedSceneRevision = scene.revision;
    }

    const QSize size = m_window->swapChainImageSize();
    VkClearColorValue clearColor = {{0.53f, 0.81f, 0.98f, 1.0f}};
    VkClearDepthStencilValue clearDepth = {1.0f, 0};
    VkClearValue clearValues[3] = {};
    clearValues[0].color = clearColor;
    clearValues[1].depthStencil = clearDepth;
    clearValues[2].color = clearColor;

    VkRenderPassBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass = m_window->defaultRenderPass();
    beginInfo.framebuffer = m_window->currentFramebuffer();
    beginInfo.renderArea.extent.width = static_cast<uint32_t>(size.width());
    beginInfo.renderArea.extent.height = static_cast<uint32_t>(size.height());
    beginInfo.clearValueCount = m_window->sampleCountFlagBits() > VK_SAMPLE_COUNT_1_BIT ? 3 : 2;
    beginInfo.pClearValues = clearValues;

    const VkCommandBuffer commandBuffer = m_window->currentCommandBuffer();
    m_functions->vkCmdBeginRenderPass(commandBuffer, &beginInfo,
                                      VK_SUBPASS_CONTENTS_INLINE);
    updateUniform(scene.camera);
    setViewport(commandBuffer, size);
    if (m_meshPipeline) {
      drawMeshes(commandBuffer, scene);
      drawAxes(commandBuffer, scene);
    }
    m_functions->vkCmdEndRenderPass(commandBuffer);
    m_window->frameReady();
  }

 private:
  VkShaderModule createShader(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
      return VK_NULL_HANDLE;
    }
    const QByteArray code = file.readAll();
    VkShaderModuleCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = static_cast<size_t>(code.size());
    info.pCode = reinterpret_cast<const uint32_t*>(code.constData());
    VkShaderModule shader = VK_NULL_HANDLE;
    return m_functions->vkCreateShaderModule(m_window->device(), &info, nullptr,
                                              &shader) == VK_SUCCESS
               ? shader
               : VK_NULL_HANDLE;
  }

  bool createBuffer(const void* data, VkDeviceSize size, VkBufferUsageFlags usage,
                    GpuBuffer& destination) {
    destroyBuffer(destination);
    if (size == 0) {
      return false;
    }
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    if (m_functions->vkCreateBuffer(m_window->device(), &bufferInfo, nullptr,
                                    &destination.buffer) != VK_SUCCESS) {
      return false;
    }
    VkMemoryRequirements requirements = {};
    m_functions->vkGetBufferMemoryRequirements(m_window->device(), destination.buffer,
                                                &requirements);
    VkMemoryAllocateInfo allocation = {};
    allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = m_window->hostVisibleMemoryIndex();
    if (m_functions->vkAllocateMemory(m_window->device(), &allocation, nullptr,
                                      &destination.memory) != VK_SUCCESS ||
        m_functions->vkBindBufferMemory(m_window->device(), destination.buffer,
                                        destination.memory, 0) != VK_SUCCESS) {
      destroyBuffer(destination);
      return false;
    }
    if (data) {
      void* mapped = nullptr;
      if (m_functions->vkMapMemory(m_window->device(), destination.memory, 0, size,
                                   0, &mapped) != VK_SUCCESS) {
        destroyBuffer(destination);
        return false;
      }
      std::memcpy(mapped, data, static_cast<size_t>(size));
      m_functions->vkUnmapMemory(m_window->device(), destination.memory);
    }
    destination.size = size;
    return true;
  }

  void destroyBuffer(GpuBuffer& buffer) {
    if (!m_functions) {
      return;
    }
    const VkDevice device = m_window->device();
    if (buffer.buffer) m_functions->vkDestroyBuffer(device, buffer.buffer, nullptr);
    if (buffer.memory) m_functions->vkFreeMemory(device, buffer.memory, nullptr);
    buffer = {};
  }

  void createUniformResources() {
    const int frames = m_window->concurrentFrameCount();
    const VkDeviceSize alignment =
        m_window->physicalDeviceProperties()->limits.minUniformBufferOffsetAlignment;
    m_uniformStride = (sizeof(GlobalUniform) + alignment - 1) & ~(alignment - 1);
    createBuffer(nullptr, m_uniformStride * frames, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                 m_uniformBuffer);
    for (int i = 0; i < frames; ++i) {
      m_uniformInfo[i].buffer = m_uniformBuffer.buffer;
      m_uniformInfo[i].offset = m_uniformStride * i;
      m_uniformInfo[i].range = sizeof(GlobalUniform);
    }
  }

  void createDescriptorResources() {
    const uint32_t frames = static_cast<uint32_t>(m_window->concurrentFrameCount());
    VkDescriptorPoolSize poolSize = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frames};
    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = frames;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    m_functions->vkCreateDescriptorPool(m_window->device(), &poolInfo, nullptr,
                                        &m_descriptorPool);

    VkDescriptorSetLayoutBinding binding = {};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;
    m_functions->vkCreateDescriptorSetLayout(m_window->device(), &layoutInfo, nullptr,
                                             &m_descriptorLayout);

    for (uint32_t i = 0; i < frames; ++i) {
      VkDescriptorSetAllocateInfo allocate = {};
      allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
      allocate.descriptorPool = m_descriptorPool;
      allocate.descriptorSetCount = 1;
      allocate.pSetLayouts = &m_descriptorLayout;
      m_functions->vkAllocateDescriptorSets(m_window->device(), &allocate,
                                            &m_descriptorSets[i]);
      VkWriteDescriptorSet write = {};
      write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write.dstSet = m_descriptorSets[i];
      write.dstBinding = 0;
      write.descriptorCount = 1;
      write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      write.pBufferInfo = &m_uniformInfo[i];
      m_functions->vkUpdateDescriptorSets(m_window->device(), 1, &write, 0, nullptr);
    }
  }

  VkPipeline createPipeline(VkPrimitiveTopology topology, bool depthBias) {
    VkShaderModule vertexShader = createShader(":/vulkan/mesh.vert.spv");
    VkShaderModule fragmentShader = createShader(":/vulkan/mesh.frag.spv");
    if (!vertexShader || !fragmentShader) {
      if (vertexShader) m_functions->vkDestroyShaderModule(m_window->device(), vertexShader, nullptr);
      if (fragmentShader) m_functions->vkDestroyShaderModule(m_window->device(), fragmentShader, nullptr);
      return VK_NULL_HANDLE;
    }

    VkPipelineShaderStageCreateInfo shaderStages[2] = {};
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vertexShader;
    shaderStages[0].pName = "main";
    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fragmentShader;
    shaderStages[1].pName = "main";

    VkVertexInputBindingDescription binding = {0, static_cast<uint32_t>(sizeof(Data::Vertex)),
                                               VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attributes[2] = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT,
         static_cast<uint32_t>(offsetof(Data::Vertex, x))},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT,
         static_cast<uint32_t>(offsetof(Data::Vertex, nx))},
    };
    VkPipelineVertexInputStateCreateInfo vertexInput = {};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = 2;
    vertexInput.pVertexAttributeDescriptions = attributes;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = topology;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0f;
    rasterization.depthBiasEnable = depthBias ? VK_TRUE : VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = m_window->sampleCountFlagBits();

    VkPipelineDepthStencilStateCreateInfo depth = {};
    depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth.depthTestEnable = VK_TRUE;
    depth.depthWriteEnable = VK_TRUE;
    depth.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkPipelineColorBlendAttachmentState blendAttachment = {};
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo blend = {};
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 1;
    blend.pAttachments = &blendAttachment;

    VkDynamicState dynamics[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
                                 VK_DYNAMIC_STATE_DEPTH_BIAS};
    VkPipelineDynamicStateCreateInfo dynamic = {};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = 3;
    dynamic.pDynamicStates = dynamics;

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewport;
    pipelineInfo.pRasterizationState = &rasterization;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pDepthStencilState = &depth;
    pipelineInfo.pColorBlendState = &blend;
    pipelineInfo.pDynamicState = &dynamic;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = m_window->defaultRenderPass();

    VkPipeline pipeline = VK_NULL_HANDLE;
    m_functions->vkCreateGraphicsPipelines(m_window->device(), m_pipelineCache, 1,
                                           &pipelineInfo, nullptr, &pipeline);
    m_functions->vkDestroyShaderModule(m_window->device(), vertexShader, nullptr);
    m_functions->vkDestroyShaderModule(m_window->device(), fragmentShader, nullptr);
    return pipeline;
  }

  void createPipelines() {
    VkPipelineCacheCreateInfo cacheInfo = {};
    cacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    m_functions->vkCreatePipelineCache(m_window->device(), &cacheInfo, nullptr,
                                       &m_pipelineCache);

    VkPushConstantRange pushRange = {};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(PushConstants);
    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &m_descriptorLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;
    m_functions->vkCreatePipelineLayout(m_window->device(), &layoutInfo, nullptr,
                                        &m_pipelineLayout);
    m_meshPipeline = createPipeline(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, false);
    m_facePipeline = createPipeline(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, true);
    m_axisPipeline = createPipeline(VK_PRIMITIVE_TOPOLOGY_LINE_LIST, false);
  }

  void updateUniform(const CameraState& camera) {
    QMatrix4x4 projection = m_window->clipCorrectionMatrix();
    const QSize size = m_window->swapChainImageSize();
    projection.perspective(45.0f,
                           static_cast<float>(std::max(size.width(), 1)) /
                               std::max(size.height(), 1),
                           0.05f, 100000.0f);
    const float yaw = camera.yawDeg * kDegToRad;
    const float pitch = camera.pitchDeg * kDegToRad;
    const float cp = std::cos(pitch);
    const QVector3D position = camera.target +
                               QVector3D(camera.distance * cp * std::cos(yaw),
                                         camera.distance * std::sin(pitch),
                                         camera.distance * cp * std::sin(yaw));
    QMatrix4x4 view;
    view.lookAt(position, camera.target, QVector3D(0, 1, 0));
    const GlobalUniform uniform = {projection * view};
    const int frame = m_window->currentFrame();
    void* mapped = nullptr;
    if (m_functions->vkMapMemory(m_window->device(), m_uniformBuffer.memory,
                                 m_uniformStride * frame, sizeof(GlobalUniform), 0,
                                 &mapped) == VK_SUCCESS) {
      std::memcpy(mapped, &uniform, sizeof(uniform));
      m_functions->vkUnmapMemory(m_window->device(), m_uniformBuffer.memory);
    }
  }

  void setViewport(VkCommandBuffer commandBuffer, const QSize& size) {
    VkViewport viewport = {};
    viewport.width = static_cast<float>(size.width());
    viewport.height = static_cast<float>(size.height());
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    m_functions->vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    VkRect2D scissor = {};
    scissor.extent.width = static_cast<uint32_t>(size.width());
    scissor.extent.height = static_cast<uint32_t>(size.height());
    m_functions->vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
  }

  static PushConstants makePushConstants(const QVector3D& offset,
                                         const QVector3D& color) {
    PushConstants constants;
    constants.model.setToIdentity();
    constants.model.translate(offset);
    constants.color[0] = color.x();
    constants.color[1] = color.y();
    constants.color[2] = color.z();
    constants.color[3] = 1.0f;
    return constants;
  }

  void bindCommon(VkCommandBuffer commandBuffer, VkPipeline pipeline) {
    const int frame = m_window->currentFrame();
    m_functions->vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                   pipeline);
    m_functions->vkCmdBindDescriptorSets(commandBuffer,
                                         VK_PIPELINE_BIND_POINT_GRAPHICS,
                                         m_pipelineLayout, 0, 1,
                                         &m_descriptorSets[frame], 0, nullptr);
  }

  void drawMeshes(VkCommandBuffer commandBuffer, const RenderScene& scene) {
    bindCommon(commandBuffer, m_meshPipeline);
    for (const GpuMesh& mesh : m_meshes) {
      const bool selected = mesh.id == scene.selectedObjectId;
      const PushConstants constants = makePushConstants(
          mesh.offset, selected ? QVector3D(0.96f, 0.62f, 0.20f)
                                : QVector3D(0.90f, 0.72f, 0.34f));
      m_functions->vkCmdPushConstants(commandBuffer, m_pipelineLayout,
                                      VK_SHADER_STAGE_VERTEX_BIT |
                                          VK_SHADER_STAGE_FRAGMENT_BIT,
                                      0, sizeof(constants), &constants);
      VkDeviceSize vertexOffset = 0;
      m_functions->vkCmdBindVertexBuffers(commandBuffer, 0, 1,
                                          &mesh.vertices.buffer, &vertexOffset);
      m_functions->vkCmdBindIndexBuffer(commandBuffer, mesh.indices.buffer, 0,
                                        VK_INDEX_TYPE_UINT32);
      m_functions->vkCmdDrawIndexed(commandBuffer, mesh.indexCount, 1, 0, 0, 0);
    }

    if (scene.selectedObjectId == 0 || scene.selectedFace < 0 ||
        !m_facePipeline) {
      return;
    }
    const auto selected = std::find_if(m_meshes.begin(), m_meshes.end(),
                                       [&scene](const GpuMesh& mesh) {
                                         return mesh.id == scene.selectedObjectId;
                                       });
    if (selected == m_meshes.end() ||
        scene.selectedFace >= static_cast<int>(selected->faceRanges.size())) {
      return;
    }
    bindCommon(commandBuffer, m_facePipeline);
    m_functions->vkCmdSetDepthBias(commandBuffer, -1.0f, 0.0f, -1.0f);
    const PushConstants constants =
        makePushConstants(selected->offset, QVector3D(0.16f, 0.58f, 0.94f));
    m_functions->vkCmdPushConstants(commandBuffer, m_pipelineLayout,
                                    VK_SHADER_STAGE_VERTEX_BIT |
                                        VK_SHADER_STAGE_FRAGMENT_BIT,
                                    0, sizeof(constants), &constants);
    VkDeviceSize vertexOffset = 0;
    m_functions->vkCmdBindVertexBuffers(commandBuffer, 0, 1,
                                        &selected->vertices.buffer, &vertexOffset);
    m_functions->vkCmdBindIndexBuffer(commandBuffer, selected->indices.buffer, 0,
                                      VK_INDEX_TYPE_UINT32);
    const Data::FaceRange& face =
        selected->faceRanges[static_cast<size_t>(scene.selectedFace)];
    m_functions->vkCmdDrawIndexed(commandBuffer, face.indexCount, 1, face.firstIndex,
                                  0, 0);
  }

  void drawAxes(VkCommandBuffer commandBuffer, const RenderScene& scene) {
    if (!m_axisVertices.buffer || !m_axisPipeline) {
      return;
    }
    bindCommon(commandBuffer, m_axisPipeline);
    VkDeviceSize vertexOffset = 0;
    m_functions->vkCmdBindVertexBuffers(commandBuffer, 0, 1,
                                        &m_axisVertices.buffer, &vertexOffset);
    const QVector3D colors[] = {QVector3D(0.93f, 0.20f, 0.22f),
                                 QVector3D(0.20f, 0.70f, 0.28f),
                                 QVector3D(0.20f, 0.42f, 0.95f)};
    for (uint32_t axis = 0; axis < 3; ++axis) {
      const PushConstants constants = makePushConstants({}, colors[axis]);
      m_functions->vkCmdPushConstants(commandBuffer, m_pipelineLayout,
                                      VK_SHADER_STAGE_VERTEX_BIT |
                                          VK_SHADER_STAGE_FRAGMENT_BIT,
                                      0, sizeof(constants), &constants);
      m_functions->vkCmdDraw(commandBuffer, 2, 1, axis * 2, 0);
    }
  }

  void rebuildMeshes(const RenderScene& scene) {
    destroyMeshes();
    m_meshes.reserve(scene.objects.size());
    float sceneRadius = 1.0f;
    for (const RenderItem& item : scene.objects) {
      GpuMesh mesh;
      mesh.id = item.id;
      mesh.offset = item.offset;
      mesh.indexCount = static_cast<uint32_t>(item.mesh.m_indices.size());
      mesh.faceRanges = item.mesh.m_faceRanges;
      if (createBuffer(item.mesh.m_vertices.data(),
                       item.mesh.m_vertices.size() * sizeof(Data::Vertex),
                       VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, mesh.vertices) &&
          createBuffer(item.mesh.m_indices.data(),
                       item.mesh.m_indices.size() * sizeof(uint32_t),
                       VK_BUFFER_USAGE_INDEX_BUFFER_BIT, mesh.indices)) {
        m_meshes.push_back(std::move(mesh));
      }
      const std::array<float, 3> center = item.mesh.center();
      for (const Data::Vertex& vertex : item.mesh.m_vertices) {
        sceneRadius = std::max(sceneRadius,
                               (QVector3D(vertex.x - center[0], vertex.y - center[1],
                                          vertex.z - center[2]))
                                   .length());
      }
    }
    const float axisLength = std::max(1.8f, sceneRadius * 1.2f);
    std::array<Data::Vertex, 6> axisVertices = {};
    axisVertices[1].x = axisLength;
    axisVertices[3].y = axisLength;
    axisVertices[5].z = axisLength;
    createBuffer(axisVertices.data(), sizeof(axisVertices),
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, m_axisVertices);
  }

  void destroyMeshes() {
    for (GpuMesh& mesh : m_meshes) {
      destroyBuffer(mesh.vertices);
      destroyBuffer(mesh.indices);
    }
    m_meshes.clear();
  }

  VulkanViewport::Window* m_window = nullptr;
  QVulkanDeviceFunctions* m_functions = nullptr;
  GpuBuffer m_uniformBuffer;
  VkDeviceSize m_uniformStride = 0;
  VkDescriptorBufferInfo m_uniformInfo[QVulkanWindow::MAX_CONCURRENT_FRAME_COUNT] = {};
  VkDescriptorSet m_descriptorSets[QVulkanWindow::MAX_CONCURRENT_FRAME_COUNT] = {};
  VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_descriptorLayout = VK_NULL_HANDLE;
  VkPipelineCache m_pipelineCache = VK_NULL_HANDLE;
  VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
  VkPipeline m_meshPipeline = VK_NULL_HANDLE;
  VkPipeline m_facePipeline = VK_NULL_HANDLE;
  VkPipeline m_axisPipeline = VK_NULL_HANDLE;
  GpuBuffer m_axisVertices;
  std::vector<GpuMesh> m_meshes;
  uint64_t m_uploadedSceneRevision = 0;
};

QVulkanWindowRenderer* VulkanViewport::Window::createRenderer() {
  return new VulkanSceneRenderer(this);
}

VulkanViewport::VulkanViewport(QWidget* parent) : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  m_instance = new QVulkanInstance;
  m_instance->setApiVersion(QVersionNumber(1, 0));
  if (!m_instance->create()) {
    m_failureReason = QString("Vulkan 初始化失败（错误码 %1）；请安装兼容的 Vulkan 驱动。")
                          .arg(m_instance->errorCode());
    auto* message = new QLabel(m_failureReason, this);
    message->setAlignment(Qt::AlignCenter);
    message->setWordWrap(true);
    layout->addWidget(message);
    delete m_instance;
    m_instance = nullptr;
    return;
  }

  m_window = new Window;
  m_window->setVulkanInstance(m_instance);
  m_windowContainer = QWidget::createWindowContainer(m_window, this);
  m_windowContainer->setFocusPolicy(Qt::StrongFocus);
  layout->addWidget(m_windowContainer);
  m_window->selectionCallback = [this](uint64_t objectId, int faceIndex) {
    emit selectionChanged(static_cast<quint64>(objectId), faceIndex);
  };
  m_window->moveStartedCallback = [this](uint64_t objectId, const QVector3D& offset) {
    emit moveStarted(static_cast<quint64>(objectId), offset);
  };
  m_window->movePreviewCallback = [this](uint64_t objectId, const QVector3D& offset) {
    emit movePreview(static_cast<quint64>(objectId), offset);
  };
  m_window->moveFinishedCallback = [this](uint64_t objectId, const QVector3D& first,
                                           const QVector3D& last) {
    emit moveFinished(static_cast<quint64>(objectId), first, last);
  };
  m_window->cameraChangedCallback = [this](const CameraState&) {
    emit cameraChanged();
  };
}

VulkanViewport::~VulkanViewport() {
  delete m_windowContainer;
  m_windowContainer = nullptr;
  m_window = nullptr;
  delete m_instance;
  m_instance = nullptr;
}

bool VulkanViewport::isAvailable() const {
  return m_instance != nullptr && m_instance->isValid() && m_window != nullptr;
}

QString VulkanViewport::failureReason() const {
  return m_failureReason.isEmpty() ? "Vulkan 后端不可用。" : m_failureReason;
}

void VulkanViewport::setScene(const Data::SceneDocument* document,
                              uint64_t selectedObjectId, int selectedFace,
                              const CameraState& camera) {
  if (m_window) {
    m_window->setRenderScene(document, selectedObjectId, selectedFace, camera);
  }
}

void VulkanViewport::setInteractionMode(RenderWidget::InteractionMode mode) {
  if (m_window) {
    m_window->setInteractionMode(mode);
  }
}

void VulkanViewport::setCameraState(const CameraState& camera) {
  if (m_window) {
    m_window->setCamera(camera);
  }
}

CameraState VulkanViewport::cameraState() const {
  return m_window ? m_window->camera() : CameraState{};
}

}  // namespace Gui
