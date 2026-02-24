#ifndef RENDERWIDGET_H
#define RENDERWIDGET_H

#include <QOpenGLBuffer>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>

#include "gui_export_def.h"
#include "rendercore/RenderCore.h"
namespace Gui {
class UI_API RenderWidget : public QOpenGLWidget,
                            protected QOpenGLFunctions_3_3_Core {
  Q_OBJECT
 public:
  explicit RenderWidget(QWidget* parent = nullptr) {}
  ~RenderWidget();
  void setRenderCore(const Data::RenderCore& core);

 protected:
  void initializeGL() override;
  void resizeGL(int w, int h) override;
  void paintGL() override;

 private:
  Data::RenderCore m_core;
  QOpenGLBuffer m_vbo{QOpenGLBuffer::VertexBuffer};
  QOpenGLBuffer m_ebo{QOpenGLBuffer::IndexBuffer};
  QOpenGLVertexArrayObject m_vao;
  QOpenGLShaderProgram m_shaderProgram;
  QMatrix4x4 m_proj;
};
}  // namespace Gui

#endif  // RENDERWIDGET_H