#include "rendercore/RenderCore.h"

#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <Poly_Array1OfTriangle.hxx>
#include <Poly_Triangulation.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <array>
#include <cmath>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
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

void RenderCore::fromOCCShape(const TopoDS_Shape& shape) {
  clear();

  // 网格化形状
  BRepMesh_IncrementalMesh mesh(shape, 0.01);
  mesh.Perform();

  // 遍历所有面
  for (TopExp_Explorer explorer(shape, TopAbs_FACE); explorer.More();
       explorer.Next()) {
    // 通过指针转换将 TopoDS_Shape 转换为 TopoDS_Face
    const TopoDS_Face& face = *(const TopoDS_Face*)&(explorer.Current());
    TopLoc_Location location;

    // 获取三角网格
    Handle(Poly_Triangulation) triangulation =
        BRep_Tool::Triangulation(face, location);

    if (!triangulation.IsNull()) {
      // 获取变换
      gp_Trsf trsf = location.Transformation();

      // 获取顶点
      const Standard_Integer nodeCount = triangulation->NbNodes();
      for (Standard_Integer i = 1; i <= nodeCount; ++i) {
        gp_Pnt pnt = triangulation->Node(i).Transformed(trsf);
        m_vertices.push_back(
            {static_cast<float>(pnt.X()), static_cast<float>(pnt.Y()),
             static_cast<float>(pnt.Z()), 0.0f, 0.0f, 1.0f,  // 临时法线
             0.0f, 0.0f});                                   // 临时纹理坐标
      }

      // 获取三角形索引
      const Standard_Integer triCount = triangulation->NbTriangles();
      for (Standard_Integer i = 1; i <= triCount; ++i) {
        const Poly_Triangle& triangle = triangulation->Triangle(i);
        Standard_Integer n1, n2, n3;
        triangle.Get(n1, n2, n3);

        // 调整索引从1开始到0开始
        m_indices.push_back(static_cast<uint32_t>(n1 - 1));
        m_indices.push_back(static_cast<uint32_t>(n2 - 1));
        m_indices.push_back(static_cast<uint32_t>(n3 - 1));
      }
    }
  }

  // 计算法线 (简化版本)
  for (size_t i = 0; i + 2 < m_indices.size(); i += 3) {
    Vertex& v1 = m_vertices[m_indices[i]];
    Vertex& v2 = m_vertices[m_indices[i + 1]];
    Vertex& v3 = m_vertices[m_indices[i + 2]];

    auto cross = [](float ax, float ay, float az, float bx, float by,
                    float bz) {
      return std::array<float, 3>{ay * bz - az * by, az * bx - ax * bz,
                                  ax * by - ay * bx};
    };

    auto normalize = [](std::array<float, 3> v) {
      float len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
      if (len <= 1e-6f) return std::array<float, 3>{0.0f, 0.0f, 1.0f};
      return std::array<float, 3>{v[0] / len, v[1] / len, v[2] / len};
    };

    auto e1 = cross(v2.x - v1.x, v2.y - v1.y, v2.z - v1.z, v3.x - v1.x,
                    v3.y - v1.y, v3.z - v1.z);
    auto n = normalize(e1);

    v1.nx = n[0];
    v1.ny = n[1];
    v1.nz = n[2];

    v2.nx = n[0];
    v2.ny = n[1];
    v2.nz = n[2];

    v3.nx = n[0];
    v3.ny = n[1];
    v3.nz = n[2];
  }
}

}  // namespace Data