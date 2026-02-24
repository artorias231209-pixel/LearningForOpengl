#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "gui_export_def.h"

namespace Ui {
class MainWindow;
}

namespace Gui {
class UI_API MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow();

 private:
  Ui::MainWindow* ui;
};
}  // namespace Gui

#endif  // MAINWINDOW_H
