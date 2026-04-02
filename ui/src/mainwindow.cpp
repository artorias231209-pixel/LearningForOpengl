#include "mainwindow.h"

#include "RenderWidget.h"
#include "ui_mainwindow.h"

namespace Gui {
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
  ui->setupUi(this);

  // 连接按钮信号
  connect(ui->Rec_btn, &QPushButton::clicked, this,
          &MainWindow::on_Rec_btn_clicked);
}

MainWindow::~MainWindow() { delete ui; }

void MainWindow::on_Rec_btn_clicked() {
  // 创建一个长方体
  auto shape = m_modelingCore.createCuboid(1.0, 1.0, 1.0);

  // 从 OCC 形状生成渲染数据
  m_renderCore.fromOCCShape(shape);

  // 设置渲染核心到 RenderWidget
  ui->openGLWidget->setRenderCore(m_renderCore);

  // 刷新渲染
  ui->openGLWidget->update();
}

}  // namespace Gui