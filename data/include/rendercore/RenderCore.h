#ifndef RENDERCORE_H
#define RENDERCORE_H

#include <TopoDS_Shape.hxx>
#include <cstdint>
#include <vector>

#include "data_export_def.h"

namespace Data {
struct Vertex {
  float x, y, z;
  float nx, ny, nz;
  float u, v;
};

class PROJECT_DATA_API RenderCore {
 public:
  RenderCore() = default;
  ~RenderCore() = default;

  std::vector<Vertex> m_vertices;
  std::vector<uint32_t> m_indices;

  void clear();
  void generatePlane(float width, float height, uint32_t nx, uint32_t ny);
  void generateTriangle(float size);
  void scale(float factor);
  void fromOCCShape(const TopoDS_Shape& shape);
};

}  // namespace Data

#endif  // RENDERCORE_H
