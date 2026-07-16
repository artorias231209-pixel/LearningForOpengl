#ifndef RENDERWIDGET_H
#define RENDERWIDGET_H

#include <QMatrix4x4>
#include <QMouseEvent>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QPoint>
#include <QString>
#include <QVector3D>
#include <QWheelEvent>

#include <cstdint>

#include "gui_export_def.h"
#include "scene/SceneDocument.h"

namespace Gui {

struct CameraState {
  QVector3D target{0.0f, 0.0f, 0.0f};
  float distance = 4.0f;
  float yawDeg = 40.0f;
  float pitchDeg = 24.0f;
};

class UI_API RenderWidget : public QOpenGLWidget,
                            protected QOpenGLFunctions_3_3_Core {
  Q_OBJECT
 public:
  enum class InteractionMode {
    SelectBody,
    SelectFace,
    MoveBody,
  };

  explicit RenderWidget(QWidget* parent = nullptr);
  ~RenderWidget() override;

  void setSceneDocument(const Data::SceneDocument* document);
  void sceneChanged();
  void setInteractionMode(InteractionMode mode);
  void setSelection(uint64_t objectId, int faceIndex = -1);
  uint64_t selectedObjectId() const;
  int selectedFaceIndex() const;
  CameraState cameraState() const;
  void setCameraState(const CameraState& state);
  void fitViewToModel();

 signals:
  void statusMessageChanged(const QString& message);
  void selectionChanged(quint64 objectId, int faceIndex);
  void moveStarted(quint64 objectId, const QVector3D& initialOffset);
  void movePreview(quint64 objectId, const QVector3D& offset);
  void moveFinished(quint64 objectId, const QVector3D& initialOffset,
                    const QVector3D& finalOffset);
  void cameraChanged();

 protected:
  void initializeGL() override;
  void resizeGL(int w, int h) override;
  void paintGL() override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;

 private:
  struct PickResult {
    bool hit = false;
    uint64_t objectId = 0;
    int faceIndex = -1;
    float distance = 0.0f;
  };

  struct AxisVertex {
    QVector3D position;
    QVector3D color;
  };

  void uploadMesh(const Data::RenderCore& core);
  void uploadAxis();
  QMatrix4x4 viewMatrix() const;
  QVector3D cameraPosition() const;
  QVector3D cameraForward() const;
  QVector3D sceneCenterWorld() const;
  float sceneRadius() const;
  QVector3D objectCenterWorld(uint64_t objectId) const;
  PickResult pickAt(const QPoint& pos) const;
  bool screenRay(const QPoint& pos, QVector3D& origin,
                 QVector3D& direction) const;
  bool intersectMovePlane(const QPoint& pos, const QVector3D& planePoint,
                          const QVector3D& planeNormal,
                          QVector3D& intersection) const;
  void clearSelection();
  void emitSelection();
  void emitStatusMessage(const QString& prefix = QString());
  const Data::SceneObject* selectedObject() const;
  static QVector3D offsetOf(const Data::SceneObject& object);

  const Data::SceneDocument* m_document = nullptr;

  QOpenGLBuffer m_vbo{QOpenGLBuffer::VertexBuffer};
  QOpenGLBuffer m_ebo{QOpenGLBuffer::IndexBuffer};
  QOpenGLVertexArrayObject m_vao;
  QOpenGLShaderProgram m_meshShader;

  QOpenGLBuffer m_axisVbo{QOpenGLBuffer::VertexBuffer};
  QOpenGLVertexArrayObject m_axisVao;
  QOpenGLShaderProgram m_axisShader;
  int m_axisVertexCount = 0;

  QMatrix4x4 m_proj;
  bool m_glInitialized = false;

  InteractionMode m_mode = InteractionMode::SelectBody;
  uint64_t m_selectedObjectId = 0;
  int m_selectedFace = -1;

  CameraState m_camera;

  bool m_orbiting = false;
  bool m_movingBody = false;
  QPoint m_lastMousePos;
  QVector3D m_dragPlanePoint;
  QVector3D m_dragPlaneNormal;
  QVector3D m_dragStartWorld;
  QVector3D m_dragStartOffset;
};

}  // namespace Gui

#endif  // RENDERWIDGET_H
