#include "modelingcore/ModelingCore.h"

#include <BRepPrimAPI_MakeBox.hxx>
#include <TopoDS_Shape.hxx>

namespace Data {

TopoDS_Shape ModelingCore::createCuboid(double dx, double dy, double dz) {
  // OpenCASCADE 的 Box 使用对称方式生成，所以保持简单
  return BRepPrimAPI_MakeBox(dx, dy, dz).Shape();
}

}  // namespace Data
