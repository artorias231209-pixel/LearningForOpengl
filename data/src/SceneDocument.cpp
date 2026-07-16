#include "scene/SceneDocument.h"

#include <Standard_Failure.hxx>

#include <algorithm>
#include <cmath>

#include "modelingcore/ModelingCore.h"

namespace {

bool Equal(double first, double second) {
  return std::abs(first - second) <= 1e-9;
}

bool Equal(const Data::PrimitiveParameters& first,
           const Data::PrimitiveParameters& second) {
  return first.type == second.type && Equal(first.radius, second.radius) &&
         Equal(first.height, second.height) && Equal(first.length, second.length) &&
         Equal(first.width, second.width) && first.sides == second.sides;
}

bool Equal(const Data::Transform& first, const Data::Transform& second) {
  return Equal(first.x, second.x) && Equal(first.y, second.y) &&
         Equal(first.z, second.z);
}

TopoDS_Shape BuildShape(Data::ModelingCore& modeling,
                        const Data::PrimitiveParameters& parameters) {
  switch (parameters.type) {
    case Data::PrimitiveType::Sphere:
      return modeling.createSphere(parameters.radius);
    case Data::PrimitiveType::Cone:
      return modeling.createCone(parameters.radius, parameters.height);
    case Data::PrimitiveType::Cylinder:
      return modeling.createCylinder(parameters.radius, parameters.height);
    case Data::PrimitiveType::Prism:
      return modeling.createRegularPrism(parameters.sides, parameters.radius,
                                         parameters.height);
    case Data::PrimitiveType::Cuboid:
      return modeling.createCuboid(parameters.length, parameters.width,
                                   parameters.height);
  }
  return {};
}

}  // namespace

namespace Data {

bool TransactionHistory::commit(TransactionType type, const SceneSnapshot& before,
                                const SceneSnapshot& after) {
  if (SceneDocument::equivalent(before, after)) {
    return false;
  }
  m_undoStack.push_back({m_nextTransactionId++, type, before, after});
  m_redoStack.clear();
  return true;
}

bool TransactionHistory::canUndo() const { return !m_undoStack.empty(); }

bool TransactionHistory::canRedo() const { return !m_redoStack.empty(); }

const SceneTransaction* TransactionHistory::transactionToUndo() const {
  return canUndo() ? &m_undoStack.back() : nullptr;
}

const SceneTransaction* TransactionHistory::transactionToRedo() const {
  return canRedo() ? &m_redoStack.back() : nullptr;
}

void TransactionHistory::markUndone() {
  if (!m_undoStack.empty()) {
    m_redoStack.push_back(std::move(m_undoStack.back()));
    m_undoStack.pop_back();
  }
}

void TransactionHistory::markRedone() {
  if (!m_redoStack.empty()) {
    m_undoStack.push_back(std::move(m_redoStack.back()));
    m_redoStack.pop_back();
  }
}

void TransactionHistory::clear() {
  m_undoStack.clear();
  m_redoStack.clear();
}

bool SceneDocument::isValid(const PrimitiveParameters& parameters) {
  const auto positive = [](double value) {
    return std::isfinite(value) && value > 0.0;
  };

  switch (parameters.type) {
    case PrimitiveType::Sphere:
      return positive(parameters.radius);
    case PrimitiveType::Cone:
    case PrimitiveType::Cylinder:
      return positive(parameters.radius) && positive(parameters.height);
    case PrimitiveType::Prism:
      return parameters.sides >= 3 && parameters.sides <= 64 &&
             positive(parameters.radius) && positive(parameters.height);
    case PrimitiveType::Cuboid:
      return positive(parameters.length) && positive(parameters.width) &&
             positive(parameters.height);
  }
  return false;
}

bool SceneDocument::equivalent(const SceneSnapshot& first,
                               const SceneSnapshot& second) {
  if (first.size() != second.size()) {
    return false;
  }

  for (size_t i = 0; i < first.size(); ++i) {
    if (first[i].id != second[i].id ||
        !Equal(first[i].parameters, second[i].parameters) ||
        !Equal(first[i].transform, second[i].transform)) {
      return false;
    }
  }
  return true;
}

const std::vector<SceneObject>& SceneDocument::objects() const {
  return m_objects;
}

const SceneObject* SceneDocument::object(uint64_t id) const {
  const auto it = std::find_if(m_objects.begin(), m_objects.end(),
                               [id](const SceneObject& object) {
                                 return object.state.id == id;
                               });
  return it == m_objects.end() ? nullptr : &*it;
}

SceneSnapshot SceneDocument::snapshot() const {
  SceneSnapshot result;
  result.reserve(m_objects.size());
  for (const SceneObject& object : m_objects) {
    result.push_back(object.state);
  }
  return result;
}

bool SceneDocument::addObject(const PrimitiveParameters& parameters,
                              const Transform& transform,
                              uint64_t* createdId) {
  const SceneSnapshot before = snapshot();
  SceneObjectState state;
  state.id = m_nextObjectId;
  state.parameters = parameters;
  state.transform = transform;

  SceneObject object;
  if (!buildObject(state, object)) {
    return false;
  }

  ++m_nextObjectId;
  m_objects.push_back(std::move(object));
  m_history.commit(TransactionType::Add, before, snapshot());
  if (createdId) {
    *createdId = state.id;
  }
  return true;
}

bool SceneDocument::removeObject(uint64_t id) {
  const auto it = std::find_if(m_objects.begin(), m_objects.end(),
                               [id](const SceneObject& object) {
                                 return object.state.id == id;
                               });
  if (it == m_objects.end()) {
    return false;
  }

  const SceneSnapshot before = snapshot();
  m_objects.erase(it);
  return m_history.commit(TransactionType::Remove, before, snapshot());
}

bool SceneDocument::updateParametersPreview(
    uint64_t id, const PrimitiveParameters& parameters) {
  SceneObject* current = mutableObject(id);
  if (!current || !isValid(parameters)) {
    return false;
  }

  SceneObjectState updatedState = current->state;
  updatedState.parameters = parameters;
  SceneObject rebuilt;
  if (!buildObject(updatedState, rebuilt)) {
    return false;
  }
  *current = std::move(rebuilt);
  return true;
}

bool SceneDocument::updateTransformPreview(uint64_t id,
                                           const Transform& transform) {
  SceneObject* current = mutableObject(id);
  if (!current || !std::isfinite(transform.x) || !std::isfinite(transform.y) ||
      !std::isfinite(transform.z)) {
    return false;
  }
  current->state.transform = transform;
  return true;
}

bool SceneDocument::commitPreview(TransactionType type,
                                  const SceneSnapshot& before) {
  return m_history.commit(type, before, snapshot());
}

bool SceneDocument::undo() {
  const SceneTransaction* transaction = m_history.transactionToUndo();
  if (!transaction || !restoreSnapshot(transaction->before)) {
    return false;
  }
  m_history.markUndone();
  return true;
}

bool SceneDocument::redo() {
  const SceneTransaction* transaction = m_history.transactionToRedo();
  if (!transaction || !restoreSnapshot(transaction->after)) {
    return false;
  }
  m_history.markRedone();
  return true;
}

bool SceneDocument::canUndo() const { return m_history.canUndo(); }

bool SceneDocument::canRedo() const { return m_history.canRedo(); }

bool SceneDocument::buildObject(const SceneObjectState& state,
                                SceneObject& object) const {
  if (!isValid(state.parameters)) {
    return false;
  }

  try {
    ModelingCore modeling;
    const TopoDS_Shape shape = BuildShape(modeling, state.parameters);
    if (shape.IsNull()) {
      return false;
    }

    RenderCore mesh;
    mesh.fromOCCShape(shape);
    if (mesh.m_vertices.empty() || mesh.m_indices.empty()) {
      return false;
    }
    object.state = state;
    object.mesh = std::move(mesh);
    return true;
  } catch (const Standard_Failure&) {
    return false;
  }
}

bool SceneDocument::restoreSnapshot(const SceneSnapshot& snapshot) {
  std::vector<SceneObject> replacement;
  replacement.reserve(snapshot.size());
  uint64_t highestId = 0;
  for (const SceneObjectState& state : snapshot) {
    if (state.id == 0 || std::any_of(replacement.begin(), replacement.end(),
                                     [&state](const SceneObject& object) {
                                       return object.state.id == state.id;
                                     })) {
      return false;
    }

    SceneObject object;
    if (!buildObject(state, object)) {
      return false;
    }
    highestId = std::max(highestId, state.id);
    replacement.push_back(std::move(object));
  }

  m_objects = std::move(replacement);
  m_nextObjectId = std::max(m_nextObjectId, highestId + 1);
  return true;
}

SceneObject* SceneDocument::mutableObject(uint64_t id) {
  const auto it = std::find_if(m_objects.begin(), m_objects.end(),
                               [id](const SceneObject& object) {
                                 return object.state.id == id;
                               });
  return it == m_objects.end() ? nullptr : &*it;
}

}  // namespace Data
