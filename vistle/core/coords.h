#ifndef COORDS_H
#define COORDS_H

#include "scalar.h"
#include "shm.h"
#include "object.h"
#include "vec.h"
#include "geometry.h"
#include "normals.h"
#include "shm_obj_ref.h"
#include "export.h"

namespace vistle {

class V_COREEXPORT Coords: public Vec<Scalar,3>, virtual public GeometryInterface {
   V_OBJECT(Coords);

 public:
   typedef Vec<Scalar,3> Base;

   Coords(const Index numVertices,
             const Meta &meta=Meta());

   std::pair<Vector, Vector> getBounds() const override;
   Index getNumCoords();
   Index getNumCoords() const;
   Index getNumVertices() override;
   Index getNumVertices() const override;
   Normals::const_ptr normals() const override;
   void setNormals(Normals::const_ptr normals);
   Vector3 getVertex(Index v) const override;

   V_DATA_BEGIN(Coords);
      shm_obj_ref<Normals> normals;

      Data(const Index numVertices = 0,
            Type id = UNKNOWN, const std::string & name = "",
            const Meta &meta=Meta());
      Data(const Vec<Scalar, 3>::Data &o, const std::string &n, Type id);
      ~Data();
      static Data *create(const std::string &name="", Type id = UNKNOWN, const Index numVertices = 0,
            const Meta &meta=Meta());

   V_DATA_END(Coords);
};

//ARCHIVE_ASSUME_ABSTRACT(Coords)

} // namespace vistle
#endif

#ifdef VISTLE_IMPL
#include "coords_impl.h"
#endif
