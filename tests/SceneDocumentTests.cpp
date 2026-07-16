#include <iostream>
#include <string>

#include "scene/SceneDocument.h"

namespace {

bool Check(bool value, const std::string& message) {
  if (!value) {
    std::cerr << "FAIL: " << message << '\n';
  }
  return value;
}

bool CheckMesh(const Data::SceneObject& object, const std::string& name) {
  bool success = Check(!object.mesh.m_vertices.empty(), name + " has vertices") &&
                 Check(!object.mesh.m_indices.empty(), name + " has indices") &&
                 Check(object.mesh.m_indices.size() % 3 == 0,
                       name + " index count is triangular");
  for (uint32_t index : object.mesh.m_indices) {
    success &= Check(index < object.mesh.m_vertices.size(),
                     name + " index is in range");
  }
  return success;
}

Data::PrimitiveParameters Parameters(Data::PrimitiveType type) {
  Data::PrimitiveParameters parameters;
  parameters.type = type;
  parameters.radius = 0.75;
  parameters.height = 1.8;
  parameters.length = 1.4;
  parameters.width = 1.1;
  parameters.sides = 6;
  return parameters;
}

bool TestPrimitiveCreation() {
  bool success = true;
  for (Data::PrimitiveType type : {Data::PrimitiveType::Sphere,
                                   Data::PrimitiveType::Cone,
                                   Data::PrimitiveType::Cylinder,
                                   Data::PrimitiveType::Prism,
                                   Data::PrimitiveType::Cuboid}) {
    Data::SceneDocument document;
    uint64_t id = 0;
    success &= Check(document.addObject(Parameters(type), {}, &id),
                     "primitive is created");
    const Data::SceneObject* object = document.object(id);
    success &= Check(object != nullptr, "created object can be found");
    if (object) {
      success &= CheckMesh(*object, "primitive");
    }
  }

  Data::PrimitiveParameters invalid = Parameters(Data::PrimitiveType::Prism);
  invalid.sides = 2;
  success &= Check(!Data::SceneDocument::isValid(invalid),
                   "two-sided prism is rejected");
  invalid = Parameters(Data::PrimitiveType::Sphere);
  invalid.radius = 0.0;
  success &= Check(!Data::SceneDocument::isValid(invalid),
                   "zero radius is rejected");
  return success;
}

bool TestTransactionHistory() {
  Data::SceneDocument document;
  uint64_t sphereId = 0;
  uint64_t boxId = 0;
  bool success = Check(document.addObject(Parameters(Data::PrimitiveType::Sphere), {},
                                          &sphereId),
                       "sphere add transaction succeeds");
  success &= Check(document.addObject(Parameters(Data::PrimitiveType::Cuboid), {},
                                      &boxId),
                   "box add transaction succeeds");
  success &= Check(document.canUndo(), "undo is available after adds");
  success &= Check(document.undo(), "undo removes latest add");
  success &= Check(document.object(boxId) == nullptr, "undo restored prior scene");
  success &= Check(document.canRedo(), "redo is available after undo");
  success &= Check(document.redo(), "redo restores latest add");
  success &= Check(document.object(boxId) != nullptr, "redo restored box");

  const Data::SceneSnapshot beforeParameters = document.snapshot();
  Data::PrimitiveParameters edited = Parameters(Data::PrimitiveType::Sphere);
  edited.radius = 1.25;
  success &= Check(document.updateParametersPreview(sphereId, edited),
                   "parameter preview succeeds");
  success &= Check(document.commitPreview(Data::TransactionType::UpdateParameters,
                                          beforeParameters),
                   "parameter transaction commits");
  success &= Check(document.undo(), "undo parameter transaction succeeds");
  const Data::SceneObject* sphere = document.object(sphereId);
  success &= Check(sphere && sphere->state.parameters.radius == 0.75,
                   "undo restored parameter");
  success &= Check(document.redo(), "redo parameter transaction succeeds");
  sphere = document.object(sphereId);
  success &= Check(sphere && sphere->state.parameters.radius == 1.25,
                   "redo restored parameter");

  const Data::SceneSnapshot beforeMove = document.snapshot();
  success &= Check(document.updateTransformPreview(sphereId, {2.0, -1.0, 0.5}),
                   "move preview succeeds");
  success &= Check(document.commitPreview(Data::TransactionType::Move, beforeMove),
                   "move transaction commits");
  success &= Check(document.undo(), "undo move succeeds");
  sphere = document.object(sphereId);
  success &= Check(sphere && sphere->state.transform.x == 0.0,
                   "undo restored transform");

  const Data::SceneSnapshot beforeRedoInvalidation = document.snapshot();
  success &= Check(document.updateTransformPreview(sphereId, {3.0, 0.0, 0.0}),
                   "replacement move preview succeeds");
  success &= Check(document.commitPreview(Data::TransactionType::Move,
                                          beforeRedoInvalidation),
                   "replacement transaction commits");
  success &= Check(!document.canRedo(), "new transaction clears redo history");

  success &= Check(document.removeObject(boxId), "remove transaction succeeds");
  success &= Check(document.object(boxId) == nullptr, "box removed");
  success &= Check(document.undo(), "undo remove succeeds");
  success &= Check(document.object(boxId) != nullptr, "undo restore removed object");
  return success;
}

}  // namespace

int main() {
  const bool success = TestPrimitiveCreation() && TestTransactionHistory();
  return success ? 0 : 1;
}
