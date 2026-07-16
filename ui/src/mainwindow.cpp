#include "mainwindow.h"

#include <QAction>
#include <QActionGroup>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QKeySequence>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStatusBar>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

#include "RenderWidget.h"
#include "VulkanViewport.h"

namespace {
constexpr int kObjectIdRole = Qt::UserRole;

QWidget* CreateSpinRow(const QString& label, QDoubleSpinBox* spin,
                       QFormLayout* layout) {
  QWidget* row = new QWidget;
  QHBoxLayout* rowLayout = new QHBoxLayout(row);
  rowLayout->setContentsMargins(0, 0, 0, 0);
  rowLayout->addWidget(spin);
  layout->addRow(label, row);
  return row;
}

void ConfigureDimensionSpin(QDoubleSpinBox* spin, double value) {
  spin->setRange(0.001, 1000000.0);
  spin->setDecimals(4);
  spin->setSingleStep(0.1);
  spin->setValue(value);
  spin->setKeyboardTracking(true);
}
}  // namespace

namespace Gui {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  buildUi();
  setupToolbar();
  setupParameterEditor();

  m_openGlView->setSceneDocument(&m_document);
  connect(m_openGlView, &RenderWidget::statusMessageChanged, this,
          [this](const QString& message) { statusBar()->showMessage(message); });
  connect(m_openGlView, &RenderWidget::selectionChanged, this,
          &MainWindow::onOpenGlSelectionChanged);
  connect(m_openGlView, &RenderWidget::moveStarted, this,
          &MainWindow::onMoveStarted);
  connect(m_openGlView, &RenderWidget::movePreview, this,
          &MainWindow::onMovePreview);
  connect(m_openGlView, &RenderWidget::moveFinished, this,
          &MainWindow::onMoveFinished);
  connect(m_openGlView, &RenderWidget::cameraChanged, this, [this]() {
    if (m_vulkanView) {
      m_vulkanView->setCameraState(m_openGlView->cameraState());
    }
  });
  connect(m_vulkanView, &VulkanViewport::selectionChanged, this,
          &MainWindow::onOpenGlSelectionChanged);
  connect(m_vulkanView, &VulkanViewport::moveStarted, this,
          &MainWindow::onMoveStarted);
  connect(m_vulkanView, &VulkanViewport::movePreview, this,
          &MainWindow::onMovePreview);
  connect(m_vulkanView, &VulkanViewport::moveFinished, this,
          &MainWindow::onMoveFinished);
  connect(m_vulkanView, &VulkanViewport::cameraChanged, this, [this]() {
    m_openGlView->setCameraState(m_vulkanView->cameraState());
  });

  connect(m_objectTree, &QTreeWidget::itemSelectionChanged, this,
          &MainWindow::onTreeSelectionChanged);
  refreshScene();
  statusBar()->showMessage(
      "右键拖动旋转，滚轮缩放；参数失焦后自动提交为一笔事务。");
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
  const bool isParameterControl = watched == m_primitiveType || watched == m_radius ||
                                  watched == m_height || watched == m_length ||
                                  watched == m_width || watched == m_sides;
  if (isParameterControl && !m_populatingEditor) {
    if (event->type() == QEvent::FocusIn) {
      beginParameterPreview();
    } else if (event->type() == QEvent::FocusOut) {
      commitParameterPreview();
    }
  }
  return QMainWindow::eventFilter(watched, event);
}

void MainWindow::buildUi() {
  setWindowTitle("参数化建模");
  resize(1180, 760);

  QWidget* central = new QWidget(this);
  auto* layout = new QHBoxLayout(central);
  layout->setContentsMargins(8, 8, 8, 8);

  QWidget* sideBar = new QWidget(central);
  sideBar->setMinimumWidth(280);
  sideBar->setMaximumWidth(360);
  auto* sideLayout = new QVBoxLayout(sideBar);
  sideLayout->setContentsMargins(0, 0, 0, 0);

  auto* objectGroup = new QGroupBox("场景对象", sideBar);
  auto* objectLayout = new QVBoxLayout(objectGroup);
  m_objectTree = new QTreeWidget(objectGroup);
  m_objectTree->setHeaderLabel("对象");
  m_objectTree->setSelectionMode(QAbstractItemView::SingleSelection);
  objectLayout->addWidget(m_objectTree);
  sideLayout->addWidget(objectGroup, 1);

  m_parameterPanel = new QGroupBox("参数", sideBar);
  sideLayout->addWidget(m_parameterPanel);
  layout->addWidget(sideBar);

  m_viewStack = new QStackedWidget(central);
  m_openGlView = new RenderWidget(m_viewStack);
  m_vulkanView = new VulkanViewport(m_viewStack);
  m_viewStack->addWidget(m_openGlView);
  m_viewStack->addWidget(m_vulkanView);
  layout->addWidget(m_viewStack, 1);
  setCentralWidget(central);
}

void MainWindow::setupToolbar() {
  QToolBar* toolbar = addToolBar("建模工具");
  toolbar->setMovable(false);

  QActionGroup* modes = new QActionGroup(this);
  modes->setExclusive(true);
  QAction* selectBody = toolbar->addAction("选择实体");
  QAction* selectFace = toolbar->addAction("选择面");
  QAction* move = toolbar->addAction("移动");
  for (QAction* action : {selectBody, selectFace, move}) {
    action->setCheckable(true);
    modes->addAction(action);
  }
  selectBody->setChecked(true);
  connect(selectBody, &QAction::triggered, this, [this] {
    m_openGlView->setInteractionMode(RenderWidget::InteractionMode::SelectBody);
    m_vulkanView->setInteractionMode(RenderWidget::InteractionMode::SelectBody);
  });
  connect(selectFace, &QAction::triggered, this, [this] {
    m_openGlView->setInteractionMode(RenderWidget::InteractionMode::SelectFace);
    m_vulkanView->setInteractionMode(RenderWidget::InteractionMode::SelectFace);
  });
  connect(move, &QAction::triggered, this, [this] {
    m_openGlView->setInteractionMode(RenderWidget::InteractionMode::MoveBody);
    m_vulkanView->setInteractionMode(RenderWidget::InteractionMode::MoveBody);
  });

  toolbar->addSeparator();
  m_undoAction = toolbar->addAction("撤消");
  m_undoAction->setShortcut(QKeySequence::Undo);
  m_redoAction = toolbar->addAction("重做");
  m_redoAction->setShortcut(QKeySequence::Redo);
  m_deleteAction = toolbar->addAction("删除");
  m_deleteAction->setShortcut(QKeySequence::Delete);
  QAction* fit = toolbar->addAction("适配视图");
  connect(m_undoAction, &QAction::triggered, this, &MainWindow::undo);
  connect(m_redoAction, &QAction::triggered, this, &MainWindow::redo);
  connect(m_deleteAction, &QAction::triggered, this, &MainWindow::removeSelected);
  connect(fit, &QAction::triggered, m_openGlView, &RenderWidget::fitViewToModel);

  toolbar->addSeparator();
  toolbar->addWidget(new QLabel("渲染后端：", toolbar));
  m_backendSelector = new QComboBox(toolbar);
  m_backendSelector->addItem("OpenGL");
  m_backendSelector->addItem("Vulkan");
  toolbar->addWidget(m_backendSelector);
  connect(m_backendSelector, qOverload<int>(&QComboBox::currentIndexChanged), this,
          &MainWindow::setRenderBackend);

  QMenu* editMenu = menuBar()->addMenu("编辑");
  editMenu->addAction(m_undoAction);
  editMenu->addAction(m_redoAction);
  editMenu->addAction(m_deleteAction);
}

void MainWindow::setupParameterEditor() {
  auto* form = new QFormLayout(m_parameterPanel);
  m_primitiveType = new QComboBox(m_parameterPanel);
  m_primitiveType->addItem("球", static_cast<int>(Data::PrimitiveType::Sphere));
  m_primitiveType->addItem("圆锥", static_cast<int>(Data::PrimitiveType::Cone));
  m_primitiveType->addItem("圆柱", static_cast<int>(Data::PrimitiveType::Cylinder));
  m_primitiveType->addItem("正多棱柱", static_cast<int>(Data::PrimitiveType::Prism));
  m_primitiveType->addItem("长方体", static_cast<int>(Data::PrimitiveType::Cuboid));
  form->addRow("图元类型", m_primitiveType);

  m_radius = new QDoubleSpinBox(m_parameterPanel);
  m_height = new QDoubleSpinBox(m_parameterPanel);
  m_length = new QDoubleSpinBox(m_parameterPanel);
  m_width = new QDoubleSpinBox(m_parameterPanel);
  m_sides = new QDoubleSpinBox(m_parameterPanel);
  ConfigureDimensionSpin(m_radius, 0.5);
  ConfigureDimensionSpin(m_height, 1.0);
  ConfigureDimensionSpin(m_length, 1.0);
  ConfigureDimensionSpin(m_width, 1.0);
  m_sides->setRange(3.0, 64.0);
  m_sides->setDecimals(0);
  m_sides->setSingleStep(1.0);
  m_sides->setValue(6.0);
  m_radiusRow = CreateSpinRow("半径", m_radius, form);
  m_heightRow = CreateSpinRow("高度", m_height, form);
  m_lengthRow = CreateSpinRow("长度", m_length, form);
  m_widthRow = CreateSpinRow("宽度", m_width, form);
  m_sidesRow = CreateSpinRow("边数", m_sides, form);

  QPushButton* add = new QPushButton("新建图元", m_parameterPanel);
  form->addRow(add);
  connect(add, &QPushButton::clicked, this, &MainWindow::addPrimitive);

  for (QObject* control : {static_cast<QObject*>(m_primitiveType),
                           static_cast<QObject*>(m_radius),
                           static_cast<QObject*>(m_height),
                           static_cast<QObject*>(m_length),
                           static_cast<QObject*>(m_width),
                           static_cast<QObject*>(m_sides)}) {
    control->installEventFilter(this);
  }
  connect(m_primitiveType, qOverload<int>(&QComboBox::currentIndexChanged), this,
          [this] {
            updateParameterVisibility();
            previewParameters();
          });
  for (QDoubleSpinBox* spin : {m_radius, m_height, m_length, m_width, m_sides}) {
    connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this](double) { previewParameters(); });
  }
  updateParameterVisibility();
}

void MainWindow::updateParameterVisibility() {
  const auto type = static_cast<Data::PrimitiveType>(
      m_primitiveType->currentData().toInt());
  m_radiusRow->setVisible(type == Data::PrimitiveType::Sphere ||
                          type == Data::PrimitiveType::Cone ||
                          type == Data::PrimitiveType::Cylinder ||
                          type == Data::PrimitiveType::Prism);
  m_heightRow->setVisible(type != Data::PrimitiveType::Sphere);
  m_lengthRow->setVisible(type == Data::PrimitiveType::Cuboid);
  m_widthRow->setVisible(type == Data::PrimitiveType::Cuboid);
  m_sidesRow->setVisible(type == Data::PrimitiveType::Prism);
}

void MainWindow::refreshScene(bool rebuildObjectTree) {
  if (m_selectedObjectId != 0 && !m_document.object(m_selectedObjectId)) {
    m_selectedObjectId = 0;
    m_selectedFace = -1;
  }
  if (rebuildObjectTree) {
    refreshObjectTree();
  }
  populateParameters(m_document.object(m_selectedObjectId));
  refreshRenderers();
  refreshHistoryActions();
}

void MainWindow::refreshRenderers() {
  m_openGlView->sceneChanged();
  m_openGlView->setSelection(m_selectedObjectId, m_selectedFace);
  m_vulkanView->setScene(&m_document, m_selectedObjectId, m_selectedFace,
                         m_openGlView->cameraState());
}

void MainWindow::refreshObjectTree() {
  QSignalBlocker blocker(m_objectTree);
  m_objectTree->clear();
  for (const Data::SceneObject& object : m_document.objects()) {
    auto* item = new QTreeWidgetItem(m_objectTree);
    item->setText(0, QString("%1 %2")
                         .arg(primitiveName(object.state.parameters.type))
                         .arg(object.state.id));
    item->setData(0, kObjectIdRole,
                  QVariant::fromValue<qulonglong>(object.state.id));
    if (object.state.id == m_selectedObjectId) {
      item->setSelected(true);
    }
  }
}

void MainWindow::refreshHistoryActions() {
  m_undoAction->setEnabled(m_document.canUndo());
  m_redoAction->setEnabled(m_document.canRedo());
  m_deleteAction->setEnabled(m_selectedObjectId != 0);
}

void MainWindow::setSelection(uint64_t objectId, int faceIndex) {
  if (m_syncingSelection) {
    return;
  }
  m_syncingSelection = true;
  if (!m_document.object(objectId)) {
    objectId = 0;
    faceIndex = -1;
  }
  m_selectedObjectId = objectId;
  m_selectedFace = faceIndex;
  refreshObjectTree();
  populateParameters(m_document.object(objectId));
  m_openGlView->setSelection(objectId, faceIndex);
  m_vulkanView->setScene(&m_document, objectId, faceIndex,
                         m_openGlView->cameraState());
  refreshHistoryActions();
  m_syncingSelection = false;
}

void MainWindow::populateParameters(const Data::SceneObject* object) {
  m_populatingEditor = true;
  const Data::PrimitiveParameters parameters =
      object ? object->state.parameters : parametersFromEditor();
  {
    QSignalBlocker typeBlocker(m_primitiveType);
    QSignalBlocker radiusBlocker(m_radius);
    QSignalBlocker heightBlocker(m_height);
    QSignalBlocker lengthBlocker(m_length);
    QSignalBlocker widthBlocker(m_width);
    QSignalBlocker sidesBlocker(m_sides);
    m_primitiveType->setCurrentIndex(
        m_primitiveType->findData(static_cast<int>(parameters.type)));
    m_radius->setValue(parameters.radius);
    m_height->setValue(parameters.height);
    m_length->setValue(parameters.length);
    m_width->setValue(parameters.width);
    m_sides->setValue(parameters.sides);
  }
  updateParameterVisibility();
  m_populatingEditor = false;
}

Data::PrimitiveParameters MainWindow::parametersFromEditor() const {
  Data::PrimitiveParameters parameters;
  parameters.type =
      static_cast<Data::PrimitiveType>(m_primitiveType->currentData().toInt());
  parameters.radius = m_radius->value();
  parameters.height = m_height->value();
  parameters.length = m_length->value();
  parameters.width = m_width->value();
  parameters.sides = static_cast<int>(m_sides->value());
  return parameters;
}

void MainWindow::beginParameterPreview() {
  if (m_populatingEditor || m_selectedObjectId == 0 ||
      m_parameterTransactionBefore.has_value()) {
    return;
  }
  m_parameterTransactionBefore = m_document.snapshot();
}

void MainWindow::previewParameters() {
  if (m_populatingEditor || m_selectedObjectId == 0) {
    return;
  }
  beginParameterPreview();
  if (m_document.updateParametersPreview(m_selectedObjectId, parametersFromEditor())) {
    refreshRenderers();
  }
}

void MainWindow::commitParameterPreview() {
  if (!m_parameterTransactionBefore.has_value()) {
    return;
  }
  m_document.commitPreview(Data::TransactionType::UpdateParameters,
                           *m_parameterTransactionBefore);
  m_parameterTransactionBefore.reset();
  refreshScene();
}

void MainWindow::addPrimitive() {
  uint64_t createdId = 0;
  if (!m_document.addObject(parametersFromEditor(), {}, &createdId)) {
    statusBar()->showMessage("无法创建图元：请检查参数。", 4000);
    return;
  }
  setSelection(createdId);
  refreshScene();
}

void MainWindow::removeSelected() {
  if (m_selectedObjectId == 0) {
    return;
  }
  if (m_document.removeObject(m_selectedObjectId)) {
    setSelection(0);
    refreshScene();
  }
}

void MainWindow::undo() {
  if (m_document.undo()) {
    refreshScene();
  }
}

void MainWindow::redo() {
  if (m_document.redo()) {
    refreshScene();
  }
}

void MainWindow::setRenderBackend(int index) {
  if (index == 1 && !m_vulkanView->isAvailable()) {
    m_backendSelector->setCurrentIndex(0);
    statusBar()->showMessage(m_vulkanView->failureReason(), 5000);
    return;
  }
  if (index == 1) {
    m_vulkanView->setScene(&m_document, m_selectedObjectId, m_selectedFace,
                           m_openGlView->cameraState());
  }
  m_viewStack->setCurrentIndex(index);
}

void MainWindow::onTreeSelectionChanged() {
  if (m_syncingSelection) {
    return;
  }
  const QList<QTreeWidgetItem*> selected = m_objectTree->selectedItems();
  const uint64_t id = selected.empty()
                          ? 0
                          : selected.front()->data(0, kObjectIdRole).toULongLong();
  setSelection(id);
}

void MainWindow::onOpenGlSelectionChanged(quint64 objectId, int faceIndex) {
  setSelection(static_cast<uint64_t>(objectId), faceIndex);
}

void MainWindow::onMoveStarted(quint64 objectId, const QVector3D&) {
  if (objectId == m_selectedObjectId) {
    m_moveTransactionBefore = m_document.snapshot();
  }
}

void MainWindow::onMovePreview(quint64 objectId, const QVector3D& offset) {
  if (objectId != m_selectedObjectId) {
    return;
  }
  Data::Transform transform;
  transform.x = offset.x();
  transform.y = offset.y();
  transform.z = offset.z();
  if (m_document.updateTransformPreview(objectId, transform)) {
    refreshRenderers();
  }
}

void MainWindow::onMoveFinished(quint64 objectId, const QVector3D&,
                                const QVector3D&) {
  if (objectId == m_selectedObjectId && m_moveTransactionBefore.has_value()) {
    m_document.commitPreview(Data::TransactionType::Move,
                             *m_moveTransactionBefore);
  }
  m_moveTransactionBefore.reset();
  refreshScene();
}

QString MainWindow::primitiveName(Data::PrimitiveType type) {
  switch (type) {
    case Data::PrimitiveType::Sphere:
      return "球";
    case Data::PrimitiveType::Cone:
      return "圆锥";
    case Data::PrimitiveType::Cylinder:
      return "圆柱";
    case Data::PrimitiveType::Prism:
      return "正多棱柱";
    case Data::PrimitiveType::Cuboid:
      return "长方体";
  }
  return "图元";
}

}  // namespace Gui
