#include "rendercore/RenderCore.h"

#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepBndLib.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <Poly_Triangulation.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <algorithm>
#include <array>
#include <cmath>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

namespace {
constexpr float kEpsilon = 1e-6f;

std::array<float, 3> Cross3(const std::array<float, 3>& a,
                            const std::array<float, 3>& b) {
  return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
          a[0] * b[1] - a[1] * b[0]};
}

void Normalize3(float& x, float& y, float& z) {
  const float len = std::sqrt(x * x + y * y + z * z);
  if (len <= kEpsilon) {
    x = 0.0f;
    y = 0.0f;
    z = 1.0f;
    return;
  }

  x /= len;
  y /= len;
  z /= len;
}

void RebuildNormals(std::vector<Data::Vertex>& vertices,
                    const std::vector<uint32_t>& indices) {
  for (auto& v : vertices) {
    v.nx = 0.0f;
    v.ny = 0.0f;
    v.nz = 0.0f;
  }

  for (size_t i = 0; i + 2 < indices.size(); i += 3) {
    Data::Vertex& v0 = vertices[indices[i]];
    Data::Vertex& v1 = vertices[indices[i + 1]];
    Data::Vertex& v2 = vertices[indices[i + 2]];

    const std::array<float, 3> e1 = {v1.x - v0.x, v1.y - v0.y, v1.z - v0.z};
    const std::array<float, 3> e2 = {v2.x - v0.x, v2.y - v0.y, v2.z - v0.z};
    const std::array<float, 3> n = Cross3(e1, e2);

    v0.nx += n[0];
    v0.ny += n[1];
    v0.nz += n[2];
    v1.nx += n[0];
    v1.ny += n[1];
    v1.nz += n[2];
    v2.nx += n[0];
    v2.ny += n[1];
    v2.nz += n[2];
  }

  for (auto& v : vertices) {
    Normalize3(v.nx, v.ny, v.nz);
  }
}
}  // namespace

namespace Data {
void RenderCore::clear() {
  m_vertices.clear();
  m_indices.clear();
  m_faceRanges.clear();
  m_boundsMin = {0.0f, 0.0f, 0.0f};
  m_boundsMax = {0.0f, 0.0f, 0.0f};
}

void RenderCore::generatePlane(float width, float height, uint32_t nx,
                               uint32_t ny) {
  clear();

  const uint32_t sx = std::max(nx, 1u);
  const uint32_t sy = std::max(ny, 1u);

  m_vertices.reserve((sx + 1) * (sy + 1));
  m_indices.reserve(sx * sy * 6);

  for (uint32_t j = 0; j <= sy; ++j) {
    const float v = static_cast<float>(j) / static_cast<float>(sy);
    const float yPos = (v - 0.5f) * height;
    for (uint32_t i = 0; i <= sx; ++i) {
      const float u = static_cast<float>(i) / static_cast<float>(sx);
      const float xPos = (u - 0.5f) * width;
      m_vertices.push_back({xPos, yPos, 0.0f, 0.0f, 0.0f, 1.0f, u, v});
    }
  }

  for (uint32_t j = 0; j < sy; ++j) {
    for (uint32_t i = 0; i < sx; ++i) {
      const uint32_t row1 = j * (sx + 1);
      const uint32_t row2 = (j + 1) * (sx + 1);

      m_indices.push_back(row1 + i);
      m_indices.push_back(row2 + i);
      m_indices.push_back(row2 + i + 1);

      m_indices.push_back(row1 + i);
      m_indices.push_back(row2 + i + 1);
      m_indices.push_back(row1 + i + 1);
    }
  }

  m_faceRanges.push_back({0, static_cast<uint32_t>(m_indices.size())});
  updateBounds();
}

void RenderCore::generateTriangle(float size) {
  clear();

  const float edge = std::max(size, 1e-3f);
  const float h = edge * 0.8660254f;
  m_vertices = {
      {-0.5f * edge, -h / 3.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f},
      {0.5f * edge, -h / 3.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f},
      {0.0f, 2.0f * h / 3.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.5f, 1.0f},
  };
  m_indices = {0, 1, 2};
  m_faceRanges.push_back({0, static_cast<uint32_t>(m_indices.size())});
  updateBounds();
}

void RenderCore::scale(float factor) {
  for (auto& v : m_vertices) {
    v.x *= factor;
    v.y *= factor;
    v.z *= factor;
  }
  updateBounds();
}

void RenderCore::fromOCCShape(const TopoDS_Shape& shape) {
  clear();

  Bnd_Box bounds;
  BRepBndLib::Add(shape, bounds);
  Standard_Real minX = 0.0;
  Standard_Real minY = 0.0;
  Standard_Real minZ = 0.0;
  Standard_Real maxX = 0.0;
  Standard_Real maxY = 0.0;
  Standard_Real maxZ = 0.0;
  bounds.Get(minX, minY, minZ, maxX, maxY, maxZ);
  const double dx = maxX - minX;
  const double dy = maxY - minY;
  const double dz = maxZ - minZ;
  const double diagonal = std::sqrt(dx * dx + dy * dy + dz * dz);
  const double deflection = std::max(diagonal * 0.005, 1e-4);

  BRepMesh_IncrementalMesh mesher(shape, deflection, false, 0.5, true);
  mesher.Perform();

  for (TopExp_Explorer explorer(shape, TopAbs_FACE); explorer.More();
       explorer.Next()) {
    const TopoDS_Face face = TopoDS::Face(explorer.Current());
    TopLoc_Location location;
    Handle(Poly_Triangulation) triangulation =
        BRep_Tool::Triangulation(face, location);
    if (triangulation.IsNull()) {
      continue;
    }

    const gp_Trsf trsf = location.Transformation();
    const uint32_t baseIndex = static_cast<uint32_t>(m_vertices.size());
    const uint32_t faceStart = static_cast<uint32_t>(m_indices.size());

    const Standard_Integer nodeCount = triangulation->NbNodes();
    for (Standard_Integer i = 1; i <= nodeCount; ++i) {
      const gp_Pnt p = triangulation->Node(i).Transformed(trsf);
      m_vertices.push_back({static_cast<float>(p.X()), static_cast<float>(p.Y()),
                            static_cast<float>(p.Z()), 0.0f, 0.0f, 0.0f, 0.0f,
                            0.0f});
    }

    const bool reversed = face.Orientation() == TopAbs_REVERSED;
    const Standard_Integer triCount = triangulation->NbTriangles();
    for (Standard_Integer i = 1; i <= triCount; ++i) {
      Standard_Integer n1 = 0;
      Standard_Integer n2 = 0;
      Standard_Integer n3 = 0;
      triangulation->Triangle(i).Get(n1, n2, n3);
      if (reversed) {
        std::swap(n2, n3);
      }

      m_indices.push_back(baseIndex + static_cast<uint32_t>(n1 - 1));
      m_indices.push_back(baseIndex + static_cast<uint32_t>(n2 - 1));
      m_indices.push_back(baseIndex + static_cast<uint32_t>(n3 - 1));
    }

    const uint32_t faceIndexCount = static_cast<uint32_t>(m_indices.size()) - faceStart;
    if (faceIndexCount > 0) {
      m_faceRanges.push_back({faceStart, faceIndexCount});
    }
  }

  RebuildNormals(m_vertices, m_indices);

  if (m_vertices.empty()) {
    return;
  }

  updateBounds();
}

std::array<float, 3> RenderCore::center() const {
  return {0.5f * (m_boundsMin[0] + m_boundsMax[0]),
          0.5f * (m_boundsMin[1] + m_boundsMax[1]),
          0.5f * (m_boundsMin[2] + m_boundsMax[2])};
}

float RenderCore::boundingRadius() const {
  const std::array<float, 3> c = center();
  float radiusSq = 0.0f;
  for (const auto& v : m_vertices) {
    const float dx = v.x - c[0];
    const float dy = v.y - c[1];
    const float dz = v.z - c[2];
    radiusSq = std::max(radiusSq, dx * dx + dy * dy + dz * dz);
  }
  return std::sqrt(radiusSq);
}

void RenderCore::updateBounds() {
  if (m_vertices.empty()) {
    m_boundsMin = {0.0f, 0.0f, 0.0f};
    m_boundsMax = {0.0f, 0.0f, 0.0f};
    return;
  }

  m_boundsMin = {m_vertices.front().x, m_vertices.front().y, m_vertices.front().z};
  m_boundsMax = m_boundsMin;
  for (const auto& v : m_vertices) {
    m_boundsMin[0] = std::min(m_boundsMin[0], v.x);
    m_boundsMin[1] = std::min(m_boundsMin[1], v.y);
    m_boundsMin[2] = std::min(m_boundsMin[2], v.z);
    m_boundsMax[0] = std::max(m_boundsMax[0], v.x);
    m_boundsMax[1] = std::max(m_boundsMax[1], v.y);
    m_boundsMax[2] = std::max(m_boundsMax[2], v.z);
  }
}

}  // namespace Data
