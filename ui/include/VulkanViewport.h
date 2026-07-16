#ifndef VULKAN_VIEWPORT_H
#define VULKAN_VIEWPORT_H

#include <QWidget>

#include <cstdint>

#include "RenderWidget.h"
#include "scene/SceneDocument.h"

class QVulkanInstance;

namespace Gui {

class VulkanViewport : public QWidget {
  Q_OBJECT

 public:
  explicit VulkanViewport(QWidget* parent = nullptr);
  ~VulkanViewport() override;

  bool isAvailable() const;
  QString failureReason() const;
  void setScene(const Data::SceneDocument* document, uint64_t selectedObjectId,
                int selectedFace, const CameraState& camera);
  void setInteractionMode(RenderWidget::InteractionMode mode);
  void setCameraState(const CameraState& camera);
  CameraState cameraState() const;

 signals:
  void statusMessageChanged(const QString& message);
  void selectionChanged(quint64 objectId, int faceIndex);
  void moveStarted(quint64 objectId, const QVector3D& initialOffset);
  void movePreview(quint64 objectId, const QVector3D& offset);
  void moveFinished(quint64 objectId, const QVector3D& initialOffset,
                    const QVector3D& finalOffset);
  void cameraChanged();

 public:
  class Window;

 private:

  Window* m_window = nullptr;
  QWidget* m_windowContainer = nullptr;
  QVulkanInstance* m_instance = nullptr;
  QString m_failureReason;
};

}  // namespace Gui

#endif  // VULKAN_VIEWPORT_H
