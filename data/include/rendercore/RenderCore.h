#ifndef RENDERCORE_H
#define RENDERCORE_H

#include <TopoDS_Shape.hxx>
#include <array>
#include <cstdint>
#include <vector>

#include "data_export_def.h"

namespace Data {
struct Vertex {
  float x, y, z;
  float nx, ny, nz;
  float u, v;
};

struct FaceRange {
  uint32_t firstIndex = 0;
  uint32_t indexCount = 0;
};

class PROJECT_DATA_API RenderCore {
 public:
  RenderCore() = default;
  ~RenderCore() = default;

  std::vector<Vertex> m_vertices;
  std::vector<uint32_t> m_indices;
  std::vector<FaceRange> m_faceRanges;

  void clear();
  void generatePlane(float width, float height, uint32_t nx, uint32_t ny);
  void generateTriangle(float size);
  void scale(float factor);
  void fromOCCShape(const TopoDS_Shape& shape);

  std::array<float, 3> center() const;
  float boundingRadius() const;

 private:
  void updateBounds();

  std::array<float, 3> m_boundsMin{0.0f, 0.0f, 0.0f};
  std::array<float, 3> m_boundsMax{0.0f, 0.0f, 0.0f};
};

}  // namespace Data

#endif  // RENDERCORE_H
