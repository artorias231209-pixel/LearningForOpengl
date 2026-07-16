#ifndef MODELINGCORE_H
#define MODELINGCORE_H

#include <TopoDS_Shape.hxx>

#include "data_export_def.h"

namespace Data {
class PROJECT_DATA_API ModelingCore {
 public:
  ModelingCore() = default;
  ~ModelingCore() = default;

  TopoDS_Shape createCuboid(double dx, double dy, double dz);
  TopoDS_Shape createTetrahedron(double edgeLength);
  TopoDS_Shape createSphere(double radius);
  TopoDS_Shape createCone(double radius, double height);
  TopoDS_Shape createCylinder(double radius, double height);
  TopoDS_Shape createRegularPrism(int sides, double circumradius,
                                  double height);
  TopoDS_Shape createRing(double majorRadius, double minorRadius);
  TopoDS_Shape createLens(double radius, double polarAngle);
};

}  // namespace Data

#endif  // MODELINGCORE_H
