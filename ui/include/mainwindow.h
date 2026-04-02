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

 private:
  Ui::MainWindow* ui;
  Data::ModelingCore m_modelingCore;
  Data::RenderCore m_renderCore;
};
}  // namespace Gui

#endif  // MAINWINDOW_H
