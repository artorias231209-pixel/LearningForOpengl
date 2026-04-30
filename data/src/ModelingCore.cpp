#include "modelingcore/ModelingCore.h"

#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepPrimAPI_MakeTorus.hxx>
#include <algorithm>

namespace Data {

TopoDS_Shape ModelingCore::createCuboid(double dx, double dy, double dz) {
  return BRepPrimAPI_MakeBox(dx, dy, dz).Shape();
}

TopoDS_Shape ModelingCore::createSphere(double radius) {
  return BRepPrimAPI_MakeSphere(radius).Shape();
}

TopoDS_Shape ModelingCore::createRing(double majorRadius, double minorRadius) {
  const double safeMajor = std::max(majorRadius, 1e-3);
  const double safeMinor = std::max(std::min(minorRadius, safeMajor * 0.9), 1e-3);
  return BRepPrimAPI_MakeTorus(safeMajor, safeMinor).Shape();
}

TopoDS_Shape ModelingCore::createLens(double radius, double polarAngle) {
  const double safeRadius = std::max(radius, 1e-3);
  const double safeAngle = std::clamp(polarAngle, 0.05, 1.3);
  return BRepPrimAPI_MakeSphere(safeRadius, -safeAngle, safeAngle).Shape();
}

}  // namespace Data
