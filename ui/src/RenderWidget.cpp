#include "RenderWidget.h"

#include <QDebug>
#include <QVector4D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegToRad = kPi / 180.0f;
constexpr float kPickEpsilon = 1e-5f;

float ClampFloat(float value, float minValue, float maxValue) {
  return std::max(minValue, std::min(maxValue, value));
}

bool IntersectRayTriangle(const QVector3D& origin, const QVector3D& direction,
                          const QVector3D& a, const QVector3D& b,
                          const QVector3D& c, float& t) {
  const QVector3D edge1 = b - a;
  const QVector3D edge2 = c - a;
  const QVector3D pvec = QVector3D::crossProduct(direction, edge2);
  const float det = QVector3D::dotProduct(edge1, pvec);
  if (std::abs(det) < kPickEpsilon) {
    return false;
  }

  const float invDet = 1.0f / det;
  const QVector3D tvec = origin - a;
  const float u = QVector3D::dotProduct(tvec, pvec) * invDet;
  if (u < 0.0f || u > 1.0f) {
    return false;
  }

  const QVector3D qvec = QVector3D::crossProduct(tvec, edge1);
  const float v = QVector3D::dotProduct(direction, qvec) * invDet;
  if (v < 0.0f || u + v > 1.0f) {
    return false;
  }

  t = QVector3D::dotProduct(edge2, qvec) * invDet;
  return t > kPickEpsilon;
}
}  // namespace

namespace Gui {

RenderWidget::RenderWidget(QWidget* parent) : QOpenGLWidget(parent) {
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);
}

RenderWidget::~RenderWidget() {
  if (!context()) {
    return;
  }

  makeCurrent();
  m_axisVbo.destroy();
  m_axisVao.destroy();
  m_ebo.destroy();
  m_vbo.destroy();
  m_vao.destroy();
  m_meshShader.removeAllShaders();
  m_axisShader.removeAllShaders();
  doneCurrent();
}

void RenderWidget::setSceneDocument(const Data::SceneDocument* document) {
  m_document = document;
  if (!selectedObject()) {
    clearSelection();
  }
  fitViewToModel();
}

void RenderWidget::sceneChanged() {
  if (!selectedObject()) {
    clearSelection();
    emitSelection();
  }
  if (m_glInitialized && context()) {
    makeCurrent();
    uploadAxis();
    doneCurrent();
  }
  update();
}

void RenderWidget::setInteractionMode(InteractionMode mode) {
  m_mode = mode;
  emitStatusMessage("Tool changed");
}

void RenderWidget::setSelection(uint64_t objectId, int faceIndex) {
  const Data::SceneObject* object = m_document ? m_document->object(objectId) : nullptr;
  m_selectedObjectId = object ? objectId : 0;
  m_selectedFace = object ? faceIndex : -1;
  emitSelection();
  emitStatusMessage();
  update();
}

uint64_t RenderWidget::selectedObjectId() const { return m_selectedObjectId; }

int RenderWidget::selectedFaceIndex() const { return m_selectedFace; }

CameraState RenderWidget::cameraState() const { return m_camera; }

void RenderWidget::setCameraState(const CameraState& state) {
  m_camera = state;
  m_camera.distance = ClampFloat(m_camera.distance, 0.6f, 100000.0f);
  m_camera.pitchDeg = ClampFloat(m_camera.pitchDeg, -85.0f, 85.0f);
  update();
}

void RenderWidget::fitViewToModel() {
  if (!m_document || m_document->objects().empty()) {
    m_camera.target = QVector3D(0.0f, 0.0f, 0.0f);
    m_camera.distance = 4.0f;
  } else {
    m_camera.target = sceneCenterWorld();
    m_camera.distance = std::max(sceneRadius() * 3.2f, 2.8f);
  }
  m_camera.yawDeg = 42.0f;
  m_camera.pitchDeg = 24.0f;
  emit cameraChanged();
  update();
}

void RenderWidget::initializeGL() {
  initializeOpenGLFunctions();
  m_glInitialized = true;

  glEnable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glClearColor(0.53f, 0.81f, 0.98f, 1.0f);

  m_vao.create();
  m_vao.bind();
  m_vbo.create();
  m_ebo.create();
  m_vbo.bind();
  m_ebo.bind();

  if (!m_meshShader.addShaderFromSourceCode(
          QOpenGLShader::Vertex, R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTex;

uniform mat4 uModel;
uniform mat4 uViewProj;

out vec3 vWorldPos;
out vec3 vWorldNormal;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    vWorldNormal = mat3(uModel) * aNormal;
    gl_Position = uViewProj * worldPos;
})")) {
    qWarning() << "Mesh vertex shader compile failed:" << m_meshShader.log();
  }

  if (!m_meshShader.addShaderFromSourceCode(
          QOpenGLShader::Fragment, R"(#version 330 core
in vec3 vWorldPos;
in vec3 vWorldNormal;

uniform vec3 uBaseColor;
uniform vec3 uCameraPos;

out vec4 FragColor;

void main() {
    vec3 N = normalize(vWorldNormal);
    vec3 L = normalize(vec3(0.35, 0.85, 0.55));
    vec3 V = normalize(uCameraPos - vWorldPos);
    vec3 H = normalize(L + V);
    float diff = max(dot(N, L), 0.20);
    float spec = pow(max(dot(N, H), 0.0), 28.0);
    vec3 ambient = 0.28 * uBaseColor;
    vec3 diffuse = diff * uBaseColor;
    vec3 specular = vec3(0.22) * spec;
    FragColor = vec4(ambient + diffuse + specular, 1.0);
})")) {
    qWarning() << "Mesh fragment shader compile failed:" << m_meshShader.log();
  }

  if (!m_meshShader.link()) {
    qWarning() << "Mesh shader link failed:" << m_meshShader.log();
  }

  m_meshShader.bind();
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Data::Vertex),
                        reinterpret_cast<void*>(0));
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Data::Vertex),
                        reinterpret_cast<void*>(offsetof(Data::Vertex, nx)));
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Data::Vertex),
                        reinterpret_cast<void*>(offsetof(Data::Vertex, u)));
  m_meshShader.release();
  m_vao.release();
  m_vbo.release();
  m_ebo.release();

  m_axisVao.create();
  m_axisVao.bind();
  m_axisVbo.create();
  m_axisVbo.bind();

  if (!m_axisShader.addShaderFromSourceCode(
          QOpenGLShader::Vertex, R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
uniform mat4 uMVP;
out vec3 vColor;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    vColor = aColor;
})")) {
    qWarning() << "Axis vertex shader compile failed:" << m_axisShader.log();
  }

  if (!m_axisShader.addShaderFromSourceCode(
          QOpenGLShader::Fragment, R"(#version 330 core
in vec3 vColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(vColor, 1.0);
})")) {
    qWarning() << "Axis fragment shader compile failed:" << m_axisShader.log();
  }

  if (!m_axisShader.link()) {
    qWarning() << "Axis shader link failed:" << m_axisShader.log();
  }

  m_axisShader.bind();
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(AxisVertex),
                        reinterpret_cast<void*>(offsetof(AxisVertex, position)));
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(AxisVertex),
                        reinterpret_cast<void*>(offsetof(AxisVertex, color)));
  m_axisShader.release();
  m_axisVao.release();
  m_axisVbo.release();
  uploadAxis();
}

void RenderWidget::resizeGL(int w, int h) {
  const int safeW = std::max(w, 1);
  const int safeH = std::max(h, 1);
  m_proj.setToIdentity();
  m_proj.perspective(45.0f, static_cast<float>(safeW) / safeH, 0.05f,
                     100000.0f);
}

void RenderWidget::paintGL() {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  const QMatrix4x4 viewProj = m_proj * viewMatrix();
  const QVector3D camPos = cameraPosition();

  if (m_axisShader.isLinked()) {
    m_axisShader.bind();
    m_axisShader.setUniformValue("uMVP", viewProj);
    m_axisVao.bind();
    glLineWidth(2.0f);
    glDrawArrays(GL_LINES, 0, m_axisVertexCount);
    m_axisVao.release();
    m_axisShader.release();
  }

  if (!m_document || !m_meshShader.isLinked()) {
    return;
  }

  for (const Data::SceneObject& object : m_document->objects()) {
    if (object.mesh.m_vertices.empty() || object.mesh.m_indices.empty()) {
      continue;
    }
    uploadMesh(object.mesh);

    QMatrix4x4 model;
    model.translate(offsetOf(object));
    const bool selected = object.state.id == m_selectedObjectId;

    m_meshShader.bind();
    m_meshShader.setUniformValue("uModel", model);
    m_meshShader.setUniformValue("uViewProj", viewProj);
    m_meshShader.setUniformValue("uCameraPos", camPos);
    m_meshShader.setUniformValue(
        "uBaseColor", selected ? QVector3D(0.96f, 0.62f, 0.20f)
                               : QVector3D(0.90f, 0.72f, 0.34f));

    m_vao.bind();
    glDrawElements(GL_TRIANGLES,
                   static_cast<GLsizei>(object.mesh.m_indices.size()),
                   GL_UNSIGNED_INT, nullptr);

    if (selected && m_selectedFace >= 0 &&
        m_selectedFace < static_cast<int>(object.mesh.m_faceRanges.size())) {
      const Data::FaceRange& face =
          object.mesh.m_faceRanges[static_cast<size_t>(m_selectedFace)];
      m_meshShader.setUniformValue("uBaseColor", QVector3D(0.16f, 0.58f, 0.94f));
      glEnable(GL_POLYGON_OFFSET_FILL);
      glPolygonOffset(-1.0f, -1.0f);
      glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(face.indexCount),
                     GL_UNSIGNED_INT,
                     reinterpret_cast<const void*>(static_cast<size_t>(face.firstIndex) *
                                                   sizeof(uint32_t)));
      glDisable(GL_POLYGON_OFFSET_FILL);
    }

    m_vao.release();
    m_meshShader.release();
  }
}

void RenderWidget::mousePressEvent(QMouseEvent* event) {
  m_lastMousePos = event->pos();
  if (event->button() == Qt::RightButton) {
    m_orbiting = true;
    return;
  }
  if (event->button() != Qt::LeftButton) {
    return;
  }

  const PickResult pick = pickAt(event->pos());
  switch (m_mode) {
    case InteractionMode::SelectBody:
      setSelection(pick.hit ? pick.objectId : 0, -1);
      emitStatusMessage("Body selection");
      break;
    case InteractionMode::SelectFace:
      setSelection(pick.hit ? pick.objectId : 0, pick.hit ? pick.faceIndex : -1);
      emitStatusMessage("Face selection");
      break;
    case InteractionMode::MoveBody:
      if (!pick.hit) {
        clearSelection();
        emitSelection();
        update();
        return;
      }
      setSelection(pick.objectId, -1);
      m_dragPlanePoint = objectCenterWorld(pick.objectId);
      m_dragPlaneNormal = cameraForward();
      if (intersectMovePlane(event->pos(), m_dragPlanePoint, m_dragPlaneNormal,
                             m_dragStartWorld)) {
        m_movingBody = true;
        m_dragStartOffset = offsetOf(*selectedObject());
        emit moveStarted(static_cast<quint64>(m_selectedObjectId),
                          m_dragStartOffset);
      }
      emitStatusMessage("Move");
      break;
  }
}

void RenderWidget::mouseMoveEvent(QMouseEvent* event) {
  const QPoint delta = event->pos() - m_lastMousePos;
  m_lastMousePos = event->pos();

  if (m_orbiting) {
    m_camera.yawDeg += static_cast<float>(delta.x()) * 0.6f;
    m_camera.pitchDeg = ClampFloat(
        m_camera.pitchDeg + static_cast<float>(delta.y()) * 0.4f, -85.0f, 85.0f);
    emit cameraChanged();
    update();
    return;
  }

  if (!m_movingBody || m_selectedObjectId == 0) {
    return;
  }

  QVector3D worldPoint;
  if (intersectMovePlane(event->pos(), m_dragPlanePoint, m_dragPlaneNormal,
                         worldPoint)) {
    emit movePreview(static_cast<quint64>(m_selectedObjectId),
                     m_dragStartOffset + worldPoint - m_dragStartWorld);
  }
}

void RenderWidget::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() == Qt::RightButton) {
    m_orbiting = false;
  }
  if (event->button() == Qt::LeftButton && m_movingBody) {
    const Data::SceneObject* object = selectedObject();
    if (object) {
      emit moveFinished(static_cast<quint64>(m_selectedObjectId), m_dragStartOffset,
                        offsetOf(*object));
    }
    m_movingBody = false;
  }
}

void RenderWidget::wheelEvent(QWheelEvent* event) {
  const QPoint numDegrees = event->angleDelta() / 8;
  if (numDegrees.y() == 0) {
    return;
  }
  const float factor = std::pow(0.88f, static_cast<float>(numDegrees.y()) / 15.0f);
  m_camera.distance = ClampFloat(m_camera.distance * factor, 0.6f, 100000.0f);
  emit cameraChanged();
  update();
}

void RenderWidget::uploadMesh(const Data::RenderCore& core) {
  m_vao.bind();
  m_vbo.bind();
  m_ebo.bind();
  m_vbo.allocate(core.m_vertices.data(),
                 static_cast<int>(core.m_vertices.size() * sizeof(Data::Vertex)));
  m_ebo.allocate(core.m_indices.data(),
                 static_cast<int>(core.m_indices.size() * sizeof(uint32_t)));
}

void RenderWidget::uploadAxis() {
  if (!m_glInitialized) {
    return;
  }
  const float axisLength = std::max(1.8f, sceneRadius() * 1.2f);
  const std::array<AxisVertex, 6> vertices = {
      AxisVertex{QVector3D(0, 0, 0), QVector3D(0.93f, 0.20f, 0.22f)},
      AxisVertex{QVector3D(axisLength, 0, 0), QVector3D(0.93f, 0.20f, 0.22f)},
      AxisVertex{QVector3D(0, 0, 0), QVector3D(0.20f, 0.70f, 0.28f)},
      AxisVertex{QVector3D(0, axisLength, 0), QVector3D(0.20f, 0.70f, 0.28f)},
      AxisVertex{QVector3D(0, 0, 0), QVector3D(0.20f, 0.42f, 0.95f)},
      AxisVertex{QVector3D(0, 0, axisLength), QVector3D(0.20f, 0.42f, 0.95f)},
  };
  m_axisVao.bind();
  m_axisVbo.bind();
  m_axisVbo.allocate(vertices.data(), static_cast<int>(sizeof(vertices)));
  m_axisVertexCount = static_cast<int>(vertices.size());
}

QMatrix4x4 RenderWidget::viewMatrix() const {
  QMatrix4x4 view;
  view.lookAt(cameraPosition(), m_camera.target, QVector3D(0.0f, 1.0f, 0.0f));
  return view;
}

QVector3D RenderWidget::cameraPosition() const {
  const float yaw = m_camera.yawDeg * kDegToRad;
  const float pitch = m_camera.pitchDeg * kDegToRad;
  const float cp = std::cos(pitch);
  return m_camera.target + QVector3D(m_camera.distance * cp * std::cos(yaw),
                                     m_camera.distance * std::sin(pitch),
                                     m_camera.distance * cp * std::sin(yaw));
}

QVector3D RenderWidget::cameraForward() const {
  return (m_camera.target - cameraPosition()).normalized();
}

QVector3D RenderWidget::sceneCenterWorld() const {
  if (!m_document || m_document->objects().empty()) {
    return {};
  }
  QVector3D minPoint(std::numeric_limits<float>::max(),
                     std::numeric_limits<float>::max(),
                     std::numeric_limits<float>::max());
  QVector3D maxPoint(-std::numeric_limits<float>::max(),
                     -std::numeric_limits<float>::max(),
                     -std::numeric_limits<float>::max());
  for (const Data::SceneObject& object : m_document->objects()) {
    const QVector3D offset = offsetOf(object);
    for (const Data::Vertex& vertex : object.mesh.m_vertices) {
      const QVector3D point(vertex.x, vertex.y, vertex.z);
      minPoint.setX(std::min(minPoint.x(), point.x() + offset.x()));
      minPoint.setY(std::min(minPoint.y(), point.y() + offset.y()));
      minPoint.setZ(std::min(minPoint.z(), point.z() + offset.z()));
      maxPoint.setX(std::max(maxPoint.x(), point.x() + offset.x()));
      maxPoint.setY(std::max(maxPoint.y(), point.y() + offset.y()));
      maxPoint.setZ(std::max(maxPoint.z(), point.z() + offset.z()));
    }
  }
  return 0.5f * (minPoint + maxPoint);
}

float RenderWidget::sceneRadius() const {
  if (!m_document || m_document->objects().empty()) {
    return 1.0f;
  }
  const QVector3D center = sceneCenterWorld();
  float radius = 1.0f;
  for (const Data::SceneObject& object : m_document->objects()) {
    const QVector3D offset = offsetOf(object);
    for (const Data::Vertex& vertex : object.mesh.m_vertices) {
      radius = std::max(radius, (QVector3D(vertex.x, vertex.y, vertex.z) + offset -
                                 center)
                                    .length());
    }
  }
  return radius;
}

QVector3D RenderWidget::objectCenterWorld(uint64_t objectId) const {
  const Data::SceneObject* object = m_document ? m_document->object(objectId) : nullptr;
  if (!object) {
    return {};
  }
  const std::array<float, 3> center = object->mesh.center();
  return QVector3D(center[0], center[1], center[2]) + offsetOf(*object);
}

RenderWidget::PickResult RenderWidget::pickAt(const QPoint& pos) const {
  PickResult best;
  if (!m_document) {
    return best;
  }
  QVector3D rayOrigin;
  QVector3D rayDirection;
  if (!screenRay(pos, rayOrigin, rayDirection)) {
    return best;
  }

  float bestDistance = std::numeric_limits<float>::max();
  for (const Data::SceneObject& object : m_document->objects()) {
    const QVector3D offset = offsetOf(object);
    for (size_t faceIndex = 0; faceIndex < object.mesh.m_faceRanges.size();
         ++faceIndex) {
      const Data::FaceRange& face = object.mesh.m_faceRanges[faceIndex];
      const uint32_t faceEnd = face.firstIndex + face.indexCount;
      for (uint32_t i = face.firstIndex; i + 2 < faceEnd; i += 3) {
        const Data::Vertex& va = object.mesh.m_vertices[object.mesh.m_indices[i]];
        const Data::Vertex& vb = object.mesh.m_vertices[object.mesh.m_indices[i + 1]];
        const Data::Vertex& vc = object.mesh.m_vertices[object.mesh.m_indices[i + 2]];
        float distance = 0.0f;
        if (!IntersectRayTriangle(rayOrigin, rayDirection,
                                  QVector3D(va.x, va.y, va.z) + offset,
                                  QVector3D(vb.x, vb.y, vb.z) + offset,
                                  QVector3D(vc.x, vc.y, vc.z) + offset,
                                  distance)) {
          continue;
        }
        if (distance < bestDistance) {
          bestDistance = distance;
          best.hit = true;
          best.objectId = object.state.id;
          best.faceIndex = static_cast<int>(faceIndex);
          best.distance = distance;
        }
      }
    }
  }
  return best;
}

bool RenderWidget::screenRay(const QPoint& pos, QVector3D& origin,
                             QVector3D& direction) const {
  if (width() <= 0 || height() <= 0) {
    return false;
  }
  const float x = 2.0f * static_cast<float>(pos.x()) / width() - 1.0f;
  const float y = 1.0f - 2.0f * static_cast<float>(pos.y()) / height();
  const QMatrix4x4 inverse = (m_proj * viewMatrix()).inverted();
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

bool RenderWidget::intersectMovePlane(const QPoint& pos,
                                      const QVector3D& planePoint,
                                      const QVector3D& planeNormal,
                                      QVector3D& intersection) const {
  QVector3D rayOrigin;
  QVector3D rayDirection;
  if (!screenRay(pos, rayOrigin, rayDirection)) {
    return false;
  }
  const float denominator = QVector3D::dotProduct(rayDirection, planeNormal);
  if (std::abs(denominator) < kPickEpsilon) {
    return false;
  }
  const float distance = QVector3D::dotProduct(planePoint - rayOrigin, planeNormal) /
                         denominator;
  if (distance < 0.0f) {
    return false;
  }
  intersection = rayOrigin + rayDirection * distance;
  return true;
}

void RenderWidget::clearSelection() {
  m_selectedObjectId = 0;
  m_selectedFace = -1;
}

void RenderWidget::emitSelection() {
  emit selectionChanged(static_cast<quint64>(m_selectedObjectId), m_selectedFace);
}

void RenderWidget::emitStatusMessage(const QString& prefix) {
  QString tool;
  switch (m_mode) {
    case InteractionMode::SelectBody:
      tool = "Select Body";
      break;
    case InteractionMode::SelectFace:
      tool = "Select Face";
      break;
    case InteractionMode::MoveBody:
      tool = "Move";
      break;
  }
  QString selected = "nothing selected";
  if (m_selectedObjectId != 0 && m_selectedFace >= 0) {
    selected = QString("object #%1, face #%2 selected")
                   .arg(m_selectedObjectId)
                   .arg(m_selectedFace + 1);
  } else if (m_selectedObjectId != 0) {
    selected = QString("object #%1 selected").arg(m_selectedObjectId);
  }
  emit statusMessageChanged(prefix.isEmpty()
                                ? QString("%1 | %2").arg(tool, selected)
                                : QString("%1 | %2 | %3").arg(prefix, tool, selected));
}

const Data::SceneObject* RenderWidget::selectedObject() const {
  return m_document && m_selectedObjectId != 0
             ? m_document->object(m_selectedObjectId)
             : nullptr;
}

QVector3D RenderWidget::offsetOf(const Data::SceneObject& object) {
  return QVector3D(static_cast<float>(object.state.transform.x),
                   static_cast<float>(object.state.transform.y),
                   static_cast<float>(object.state.transform.z));
}

}  // namespace Gui
