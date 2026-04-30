#include "mainwindow.h"

#include <QPushButton>

#include "ui_mainwindow.h"

namespace Gui {
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
  ui->setupUi(this);

  connect(ui->Rec_btn, &QPushButton::clicked, this,
          &MainWindow::on_Rec_btn_clicked);
  connect(ui->Tri_btn, &QPushButton::clicked, this,
          &MainWindow::on_Tri_btn_clicked);
  connect(ui->Sphere_btn, &QPushButton::clicked, this,
          &MainWindow::on_Sphere_btn_clicked);
  connect(ui->Lens_btn, &QPushButton::clicked, this,
          &MainWindow::on_Lens_btn_clicked);
  connect(ui->Ring_btn, &QPushButton::clicked, this,
          &MainWindow::on_Ring_btn_clicked);
}

MainWindow::~MainWindow() { delete ui; }

void MainWindow::pushToRenderWidget() {
  ui->openGLWidget->setRenderCore(m_renderCore);
  ui->openGLWidget->update();
}

void MainWindow::on_Rec_btn_clicked() {
  m_renderCore.generatePlane(1.8f, 1.2f, 1, 1);
  pushToRenderWidget();
}

void MainWindow::on_Tri_btn_clicked() {
  m_renderCore.generateTriangle(1.6f);
  pushToRenderWidget();
}

void MainWindow::on_Sphere_btn_clicked() {
  const auto shape = m_modelingCore.createSphere(0.75);
  m_renderCore.fromOCCShape(shape);
  pushToRenderWidget();
}

void MainWindow::on_Lens_btn_clicked() {
  const auto shape = m_modelingCore.createLens(0.9, 0.45);
  m_renderCore.fromOCCShape(shape);
  pushToRenderWidget();
}

void MainWindow::on_Ring_btn_clicked() {
  const auto shape = m_modelingCore.createRing(0.75, 0.24);
  m_renderCore.fromOCCShape(shape);
  pushToRenderWidget();
}

}  // namespace Gui
