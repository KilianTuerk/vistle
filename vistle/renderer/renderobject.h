#ifndef VISTLE_RENDEROBJECT_H
#define VISTLE_RENDEROBJECT_H

#include <vector>

#include <util/enum.h>
#include <core/vector.h>
#include <core/object.h>
#include <core/normals.h>
#include <core/texture1d.h>

#include "export.h"

namespace vistle {

class V_RENDEREREXPORT RenderObject {

 public:
   RenderObject(int senderId, const std::string &senderPort,
         vistle::Object::const_ptr container,
         vistle::Object::const_ptr geometry,
         vistle::Object::const_ptr normals,
         vistle::Object::const_ptr mapdata);

   virtual ~RenderObject();

   void updateBounds();
   void computeBounds();
   bool boundsValid() const;

   int creatorId;
   int senderId;
   std::string senderPort;
   std::string variant;
   DEFINE_ENUM_WITH_STRING_CONVERSIONS(InitialVariantVisibility, (DontChange)(Hidden)(Visible));
   InitialVariantVisibility visibility = DontChange;

   vistle::Object::const_ptr container;
   vistle::Object::const_ptr geometry;
   vistle::Normals::const_ptr normals;
   vistle::Texture1D::const_ptr texture;
   vistle::Vec<Scalar>::const_ptr scalars;

   int timestep = -1;
   bool bComputed = false;
   bool bValid = false;
   vistle::Vector bMin, bMax;

   bool hasSolidColor = false;
   vistle::Vector4 solidColor;
};

}
#endif
