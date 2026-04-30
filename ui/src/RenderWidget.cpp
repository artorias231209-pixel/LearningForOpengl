#include "RenderWidget.h"

#include <cstddef>
#include <QDebug>

namespace Gui {
RenderWidget::RenderWidget(QWidget* parent) : QOpenGLWidget(parent) {}

RenderWidget::~RenderWidget() {
  if (!context()) {
    return;
  }

  makeCurrent();
  m_ebo.destroy();
  m_vbo.destroy();
  m_vao.destroy();
  m_shaderProgram.release();
  m_shaderProgram.removeAllShaders();
  doneCurrent();
}

void RenderWidget::setRenderCore(const Data::RenderCore& core) {
  m_core = core;

  if (!m_glInitialized || !context()) {
    return;
  }

  makeCurrent();
  uploadMesh();
  doneCurrent();
}

void RenderWidget::initializeGL() {
  initializeOpenGLFunctions();
  m_glInitialized = true;

  glEnable(GL_DEPTH_TEST);
  glClearColor(0.08f, 0.09f, 0.12f, 1.0f);

  m_vao.create();
  m_vao.bind();

  m_vbo.create();
  m_ebo.create();
  m_vbo.bind();
  m_ebo.bind();

  if (!m_shaderProgram.addShaderFromSourceCode(
          QOpenGLShader::Vertex, R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTex;

out vec3 vNormal;
uniform mat4 uMVP;

void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    vNormal = aNormal;
})")) {
    qWarning() << "Vertex shader compile failed:" << m_shaderProgram.log();
  }

  if (!m_shaderProgram.addShaderFromSourceCode(
          QOpenGLShader::Fragment, R"(#version 330 core
in vec3 vNormal;
out vec4 FragColor;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(vec3(0.35, 0.75, 0.55));
    float diff = max(dot(N, L), 0.15);
    vec3 base = vec3(0.96, 0.70, 0.32);
    FragColor = vec4(base * diff, 1.0);
})")) {
    qWarning() << "Fragment shader compile failed:" << m_shaderProgram.log();
  }

  if (!m_shaderProgram.link()) {
    qWarning() << "Shader link failed:" << m_shaderProgram.log();
  }
  m_shaderProgram.bind();

  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glEnableVertexAttribArray(2);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Data::Vertex),
                        reinterpret_cast<void*>(0));
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Data::Vertex),
                        reinterpret_cast<void*>(offsetof(Data::Vertex, nx)));
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Data::Vertex),
                        reinterpret_cast<void*>(offsetof(Data::Vertex, u)));

  uploadMesh();

  m_vao.release();
  m_vbo.release();
  m_ebo.release();
  m_shaderProgram.release();
}

void RenderWidget::resizeGL(int w, int h) {
  const int safeH = (h <= 0) ? 1 : h;
  m_proj.setToIdentity();
  m_proj.perspective(45.0f, static_cast<float>(w) / static_cast<float>(safeH),
                     0.1f, 100.0f);
}

void RenderWidget::paintGL() {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  if (m_core.m_vertices.empty() || m_core.m_indices.empty() ||
      !m_shaderProgram.isLinked()) {
    return;
  }

  m_shaderProgram.bind();
  m_vao.bind();

  QMatrix4x4 model;
  model.rotate(24.0f, 1.0f, 1.0f, 0.0f);

  QMatrix4x4 view;
  view.translate(0.0f, 0.0f, -3.0f);

  const QMatrix4x4 mvp = m_proj * view * model;
  m_shaderProgram.setUniformValue("uMVP", mvp);

  glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_core.m_indices.size()),
                 GL_UNSIGNED_INT, nullptr);

  m_vao.release();
  m_shaderProgram.release();
}

void RenderWidget::uploadMesh() {
  m_vao.bind();
  m_vbo.bind();
  m_ebo.bind();

  m_vbo.allocate(
      m_core.m_vertices.empty() ? nullptr : m_core.m_vertices.data(),
      static_cast<int>(m_core.m_vertices.size() * sizeof(Data::Vertex)));
  m_ebo.allocate(
      m_core.m_indices.empty() ? nullptr : m_core.m_indices.data(),
      static_cast<int>(m_core.m_indices.size() * sizeof(uint32_t)));
}
}  // namespace Gui
