#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <QVector3D>

#include <optional>

#include "gui_export_def.h"
#include "scene/SceneDocument.h"

class QAction;
class QComboBox;
class QDoubleSpinBox;
class QEvent;
class QStackedWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QWidget;

namespace Gui {
class RenderWidget;
class VulkanViewport;

class UI_API MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override = default;

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  void buildUi();
  void setupToolbar();
  void setupParameterEditor();
  void updateParameterVisibility();
  void refreshScene(bool rebuildObjectTree = true);
  void refreshRenderers();
  void refreshObjectTree();
  void refreshHistoryActions();
  void setSelection(uint64_t objectId, int faceIndex = -1);
  void populateParameters(const Data::SceneObject* object);
  Data::PrimitiveParameters parametersFromEditor() const;
  void beginParameterPreview();
  void previewParameters();
  void commitParameterPreview();
  void addPrimitive();
  void removeSelected();
  void undo();
  void redo();
  void setRenderBackend(int index);
  void onTreeSelectionChanged();
  void onOpenGlSelectionChanged(quint64 objectId, int faceIndex);
  void onMoveStarted(quint64 objectId, const QVector3D& offset);
  void onMovePreview(quint64 objectId, const QVector3D& offset);
  void onMoveFinished(quint64 objectId, const QVector3D& initial,
                      const QVector3D& final);
  static QString primitiveName(Data::PrimitiveType type);

  Data::SceneDocument m_document;
  uint64_t m_selectedObjectId = 0;
  int m_selectedFace = -1;
  bool m_syncingSelection = false;
  bool m_populatingEditor = false;
  std::optional<Data::SceneSnapshot> m_parameterTransactionBefore;
  std::optional<Data::SceneSnapshot> m_moveTransactionBefore;

  QTreeWidget* m_objectTree = nullptr;
  QComboBox* m_primitiveType = nullptr;
  QDoubleSpinBox* m_radius = nullptr;
  QDoubleSpinBox* m_height = nullptr;
  QDoubleSpinBox* m_length = nullptr;
  QDoubleSpinBox* m_width = nullptr;
  QDoubleSpinBox* m_sides = nullptr;
  QWidget* m_radiusRow = nullptr;
  QWidget* m_heightRow = nullptr;
  QWidget* m_lengthRow = nullptr;
  QWidget* m_widthRow = nullptr;
  QWidget* m_sidesRow = nullptr;
  QWidget* m_parameterPanel = nullptr;

  QStackedWidget* m_viewStack = nullptr;
  RenderWidget* m_openGlView = nullptr;
  VulkanViewport* m_vulkanView = nullptr;
  QComboBox* m_backendSelector = nullptr;

  QAction* m_undoAction = nullptr;
  QAction* m_redoAction = nullptr;
  QAction* m_deleteAction = nullptr;
};
}  // namespace Gui

#endif  // MAINWINDOW_H
