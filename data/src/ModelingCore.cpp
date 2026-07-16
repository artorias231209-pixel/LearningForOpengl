#include "modelingcore/ModelingCore.h"

#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepPrimAPI_MakeTorus.hxx>
#include <algorithm>
#include <cmath>
#include <TopoDS.hxx>
#include <TopoDS_Shell.hxx>
#include <gp_Ax2.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

namespace Data {

TopoDS_Shape ModelingCore::createCuboid(double dx, double dy, double dz) {
  const double safeDx = std::max(dx, 1e-3);
  const double safeDy = std::max(dy, 1e-3);
  const double safeDz = std::max(dz, 1e-3);
  return BRepPrimAPI_MakeBox(gp_Pnt(-safeDx * 0.5, -safeDy * 0.5,
                                     -safeDz * 0.5),
                             safeDx, safeDy, safeDz)
      .Shape();
}

TopoDS_Shape ModelingCore::createTetrahedron(double edgeLength) {
  const double safeEdge = std::max(edgeLength, 1e-3);
  const double baseHeight = std::sqrt(3.0) * 0.5 * safeEdge;
  const double apexHeight = std::sqrt(2.0 / 3.0) * safeEdge;

  const gp_Pnt p0(-0.5 * safeEdge, 0.0, 0.0);
  const gp_Pnt p1(0.5 * safeEdge, 0.0, 0.0);
  const gp_Pnt p2(0.0, baseHeight, 0.0);
  const gp_Pnt p3(0.0, baseHeight / 3.0, apexHeight);

  auto makeFace = [](const gp_Pnt& a, const gp_Pnt& b, const gp_Pnt& c) {
    BRepBuilderAPI_MakePolygon polygon;
    polygon.Add(a);
    polygon.Add(b);
    polygon.Add(c);
    polygon.Close();
    return BRepBuilderAPI_MakeFace(polygon.Wire()).Face();
  };

  BRepBuilderAPI_Sewing sewing;
  sewing.Add(makeFace(p0, p1, p2));
  sewing.Add(makeFace(p0, p3, p1));
  sewing.Add(makeFace(p1, p3, p2));
  sewing.Add(makeFace(p2, p3, p0));
  sewing.Perform();

  const TopoDS_Shell shell = TopoDS::Shell(sewing.SewedShape());
  return BRepBuilderAPI_MakeSolid(shell).Solid();
}

TopoDS_Shape ModelingCore::createSphere(double radius) {
  return BRepPrimAPI_MakeSphere(std::max(radius, 1e-3)).Shape();
}

TopoDS_Shape ModelingCore::createCone(double radius, double height) {
  const double safeRadius = std::max(radius, 1e-3);
  const double safeHeight = std::max(height, 1e-3);
  const gp_Ax2 axis(gp_Pnt(0.0, 0.0, -safeHeight * 0.5),
                    gp_Dir(0.0, 0.0, 1.0));
  return BRepPrimAPI_MakeCone(axis, safeRadius, 0.0, safeHeight).Shape();
}

TopoDS_Shape ModelingCore::createCylinder(double radius, double height) {
  const double safeRadius = std::max(radius, 1e-3);
  const double safeHeight = std::max(height, 1e-3);
  const gp_Ax2 axis(gp_Pnt(0.0, 0.0, -safeHeight * 0.5),
                    gp_Dir(0.0, 0.0, 1.0));
  return BRepPrimAPI_MakeCylinder(axis, safeRadius, safeHeight).Shape();
}

TopoDS_Shape ModelingCore::createRegularPrism(int sides, double circumradius,
                                               double height) {
  constexpr double kPi = 3.14159265358979323846;
  const int safeSides = std::clamp(sides, 3, 64);
  const double safeRadius = std::max(circumradius, 1e-3);
  const double safeHeight = std::max(height, 1e-3);

  BRepBuilderAPI_MakePolygon polygon;
  for (int i = 0; i < safeSides; ++i) {
    const double angle = 2.0 * kPi * static_cast<double>(i) /
                         static_cast<double>(safeSides);
    polygon.Add(gp_Pnt(safeRadius * std::cos(angle),
                       safeRadius * std::sin(angle), -safeHeight * 0.5));
  }
  polygon.Close();
  const TopoDS_Face base = BRepBuilderAPI_MakeFace(polygon.Wire()).Face();
  return BRepPrimAPI_MakePrism(base, gp_Vec(0.0, 0.0, safeHeight)).Shape();
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
