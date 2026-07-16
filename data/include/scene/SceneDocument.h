#ifndef SCENE_DOCUMENT_H
#define SCENE_DOCUMENT_H

#include <cstdint>
#include <vector>

#include "data_export_def.h"
#include "rendercore/RenderCore.h"

namespace Data {

enum class PrimitiveType : uint8_t {
  Sphere,
  Cone,
  Cylinder,
  Prism,
  Cuboid,
};

struct PrimitiveParameters {
  PrimitiveType type = PrimitiveType::Sphere;
  double radius = 0.5;
  double height = 1.0;
  double length = 1.0;
  double width = 1.0;
  int sides = 6;
};

struct Transform {
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
};

struct SceneObjectState {
  uint64_t id = 0;
  PrimitiveParameters parameters;
  Transform transform;
};

using SceneSnapshot = std::vector<SceneObjectState>;

enum class TransactionType : uint8_t {
  Add,
  Remove,
  UpdateParameters,
  Move,
};

struct SceneTransaction {
  uint64_t id = 0;
  TransactionType type = TransactionType::Add;
  SceneSnapshot before;
  SceneSnapshot after;
};

class PROJECT_DATA_API TransactionHistory {
 public:
  bool commit(TransactionType type, const SceneSnapshot& before,
              const SceneSnapshot& after);
  bool canUndo() const;
  bool canRedo() const;
  const SceneTransaction* transactionToUndo() const;
  const SceneTransaction* transactionToRedo() const;
  void markUndone();
  void markRedone();
  void clear();

 private:
  uint64_t m_nextTransactionId = 1;
  std::vector<SceneTransaction> m_undoStack;
  std::vector<SceneTransaction> m_redoStack;
};

struct SceneObject {
  SceneObjectState state;
  RenderCore mesh;
};

class PROJECT_DATA_API SceneDocument {
 public:
  SceneDocument() = default;

  static bool isValid(const PrimitiveParameters& parameters);
  static bool equivalent(const SceneSnapshot& first,
                         const SceneSnapshot& second);

  const std::vector<SceneObject>& objects() const;
  const SceneObject* object(uint64_t id) const;
  SceneSnapshot snapshot() const;

  bool addObject(const PrimitiveParameters& parameters,
                 const Transform& transform, uint64_t* createdId = nullptr);
  bool removeObject(uint64_t id);
  bool updateParametersPreview(uint64_t id,
                               const PrimitiveParameters& parameters);
  bool updateTransformPreview(uint64_t id, const Transform& transform);
  bool commitPreview(TransactionType type, const SceneSnapshot& before);

  bool undo();
  bool redo();
  bool canUndo() const;
  bool canRedo() const;

 private:
  bool buildObject(const SceneObjectState& state, SceneObject& object) const;
  bool restoreSnapshot(const SceneSnapshot& snapshot);
  SceneObject* mutableObject(uint64_t id);

  uint64_t m_nextObjectId = 1;
  std::vector<SceneObject> m_objects;
  TransactionHistory m_history;
};

}  // namespace Data

#endif  // SCENE_DOCUMENT_H
