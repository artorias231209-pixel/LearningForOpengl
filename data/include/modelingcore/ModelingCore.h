#ifndef MODELINGCORE_H
#define MODELINGCORE_H

#include <BRepPrimAPI_MakeBox.hxx>
#include <TopoDS_Shape.hxx>

#include "data_export_def.h"

namespace Data {
class PROJECT_DATA_API ModelingCore {
 public:
  ModelingCore() = default;
  ~ModelingCore() = default;

  // 创建长方体 (cuboid)
  TopoDS_Shape createCuboid(double dx, double dy, double dz);

  // 其他建模方法可以在这里添加
};

}  // namespace Data

#endif  // MODELINGCORE_H