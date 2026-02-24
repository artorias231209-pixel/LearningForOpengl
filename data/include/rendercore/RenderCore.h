#ifndef RENDERCORE_H
#define RENDERCORE_H

#include <string>
#include <vector>

#include "data_export_def.h"

namespace Data {
struct Vertex {
  float x, y, z;     // 位置
  float nx, ny, nz;  // 法线
  float u, v;        // 纹理坐标
};

class PROJECT_DATA_API RenderCore {
 public:
  RenderCore() = default;
  ~RenderCore() = default;

  // 顶点、索引数据
  std::vector<Vertex> m_vertices;
  std::vector<uint32_t> m_indices;

  void clear();
  void generatePlane(float width, float height, uint32_t nx, uint32_t ny);
  void scale(float factor);
};

}  // namespace Data

#endif  // RENDERCORE_H