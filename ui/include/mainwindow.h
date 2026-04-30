#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "gui_export_def.h"
#include "modelingcore/ModelingCore.h"
#include "rendercore/RenderCore.h"

namespace Ui {
class MainWindow;
}

namespace Gui {
class UI_API MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow();

 private slots:
  void on_Rec_btn_clicked();
  void on_Tri_btn_clicked();
  void on_Sphere_btn_clicked();
  void on_Lens_btn_clicked();
  void on_Ring_btn_clicked();

 private:
  void pushToRenderWidget();

  Ui::MainWindow* ui = nullptr;
  Data::ModelingCore m_modelingCore;
  Data::RenderCore m_renderCore;
};
}  // namespace Gui

#endif  // MAINWINDOW_H
