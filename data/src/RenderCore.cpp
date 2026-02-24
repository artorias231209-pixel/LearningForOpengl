#include "rendercore/RenderCore.h"

#include <iostream>

namespace Data {
void RenderCore::clear() {
  m_vertices.clear();
  m_indices.clear();
}

void RenderCore::generatePlane(float width, float height, uint32_t nx,
                               uint32_t ny) {
  clear();
  m_vertices.reserve((nx + 1) * (ny + 1));
  m_indices.reserve(nx * ny * 6);

  for (uint32_t j = 0; j <= ny; ++j) {
    float v = float(j) / ny;
    float yPos = (v - 0.5f) * height;
    for (uint32_t i = 0; i <= nx; ++i) {
      float u = float(i) / nx;
      float xPos = (u - 0.5f) * width;
      m_vertices.push_back({xPos, yPos, 0.0f, 0.0f, 0.0f, 1.0f, u, v});
    }
  }

  for (uint32_t j = 0; j < ny; ++j) {
    for (uint32_t i = 0; i < nx; ++i) {
      uint32_t row1 = j * (nx + 1);
      uint32_t row2 = (j + 1) * (nx + 1);

      m_indices.push_back(row1 + i);
      m_indices.push_back(row2 + i);
      m_indices.push_back(row2 + i + 1);

      m_indices.push_back(row1 + i);
      m_indices.push_back(row2 + i + 1);
      m_indices.push_back(row1 + i + 1);
    }
  }
}

// 生成圆柱或球等方法可以继续添加
void RenderCore::scale(float factor) {
  for (auto& v : m_vertices) {
    v.x *= factor;
    v.y *= factor;
    v.z *= factor;
  }
}
}  // namespace Data