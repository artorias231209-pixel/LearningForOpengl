#include "RenderWidget.h"

namespace Gui {
RenderWidget::~RenderWidget() {
  makeCurrent();
  m_vbo.destroy();
  m_vao.destroy();
  m_shaderProgram.release();
  m_shaderProgram.removeAllShaders();
  doneCurrent();
}

void RenderWidget::setRenderCore(const Data::RenderCore& core) {
  m_core = core;
}

void RenderWidget::initializeGL() {
  initializeOpenGLFunctions();

  glEnable(GL_DEPTH_TEST);

  // 创建 VAO
  m_vao.create();
  m_vao.bind();

  // 创建 VBO
  m_vbo.create();
  m_vbo.bind();
  m_vbo.allocate(
      m_core.m_vertices.data(),
      static_cast<int>(m_core.m_vertices.size() * sizeof(Data::Vertex)));

  // 顶点属性
  m_shaderProgram.addShaderFromSourceCode(QOpenGLShader::Vertex,
                                          R"(#version 330 core
            layout(location = 0) in vec3 aPos;
            layout(location = 1) in vec3 aNormal;
            layout(location = 2) in vec2 aTex;

            out vec3 vNormal;
            out vec2 vTex;

            uniform mat4 uMVP;

            void main() {
                gl_Position = uMVP * vec4(aPos, 1.0);
                vNormal = aNormal;
                vTex = aTex;
            })");

  m_shaderProgram.addShaderFromSourceCode(QOpenGLShader::Fragment,
                                          R"(#version 330 core
            in vec3 vNormal;
            in vec2 vTex;
            out vec4 FragColor;

            void main() {
                vec3 color = normalize(vNormal) * 0.5 + 0.5;
                FragColor = vec4(color, 1.0);
            })");

  m_shaderProgram.link();
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

  // 创建 EBO
  m_ebo.create();
  m_ebo.bind();
  m_ebo.allocate(m_core.m_indices.data(),
                 static_cast<int>(m_core.m_indices.size() * sizeof(uint32_t)));

  m_vao.release();
  m_vbo.release();
  m_ebo.release();
  m_shaderProgram.release();
}

void RenderWidget::resizeGL(int w, int h) {
  m_proj.setToIdentity();
  m_proj.perspective(45.0f, float(w) / float(h), 0.1f, 100.0f);
}

void RenderWidget::paintGL() {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  m_shaderProgram.bind();
  m_vao.bind();

  QMatrix4x4 model;
  model.rotate(30.0f, 1, 1, 0);
  QMatrix4x4 view;
  view.translate(0, 0, -3);
  QMatrix4x4 mvp = m_proj * view * model;

  m_shaderProgram.setUniformValue("uMVP", mvp);

  glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_core.m_indices.size()),
                 GL_UNSIGNED_INT, nullptr);

  m_vao.release();
  m_shaderProgram.release();
}
}  // namespace Gui