#include <sstream>
#include <iomanip>
#include <cfloat>
#include <limits>
#include <core/vector.h>
#include <core/object.h>
#include <core/vec.h>
#include <core/texture1d.h>
#include <core/coords.h>
#include <util/math.h>

#include "Color.h"

MODULE_MAIN(Color)

using namespace vistle;

DEFINE_ENUM_WITH_STRING_CONVERSIONS(TransferFunction,
                                    (COVISE)
                                    (Star)
                                    (ITSM)
                                    (Rainbow)
                                    (Blue_Light)
                                    (ANSYS)
                                    (CoolWarm)
                                    )

ColorMap::ColorMap(std::map<vistle::Scalar, vistle::Vector> & pins,
                   const size_t w): width(w) {

   data = new unsigned char[width * 4];

   std::map<vistle::Scalar, vistle::Vector>::iterator current = pins.begin();
   std::map<vistle::Scalar, vistle::Vector>::iterator next = ++pins.begin();

   for (size_t index = 0; index < width; index ++) {

      if (((vistle::Scalar) index) / width > next->first) {
         if (next != pins.end()) {
            current ++;
            next ++;
         }
      }

      if (next == pins.end()) {
         data[index * 4] = current->second[0];
         data[index * 4 + 1] = current->second[1];
         data[index * 4 + 2] = current->second[2];
         data[index * 4 + 3] = 1.0;
      } else {

         vistle::Scalar a = current->first;
         vistle::Scalar b = next->first;

         vistle::Scalar t = ((index / (vistle::Scalar) width) - a) / (b - a);

         data[index * 4] =
            (lerp(current->second[0], next->second[0], t) * 255.0);
         data[index * 4 + 1] =
            (lerp(current->second[1], next->second[1], t) * 255.0);
         data[index * 4 + 2] =
            (lerp(current->second[2], next->second[2], t) * 255.0);
         data[index * 4 + 3] = 1.0;
      }
   }
}

ColorMap::~ColorMap() {

   delete[] data;
}

Color::Color(const std::string &shmname, const std::string &name, int moduleID)
   : Module("Color", shmname, name, moduleID) {

   createInputPort("data_in");
   createOutputPort("data_out");

   addFloatParameter("min", "minimum value of range to map", 0.0);
   addFloatParameter("max", "maximum value of range to map", 0.0);
   auto p = addIntParameter("map", "transfer function name", 0, Parameter::Choice);
   V_ENUM_SET_CHOICES(p, TransferFunction);

   TF pins;

   using vistle::Vector;
   pins.insert(std::make_pair(0.0, vistle::Vector(0.0, 0.0, 1.0)));
   pins.insert(std::make_pair(0.5, vistle::Vector(1.0, 0.0, 0.0)));
   pins.insert(std::make_pair(1.0, vistle::Vector(1.0, 1.0, 0.0)));
   transferFunctions[COVISE] = pins;
   pins.clear();

   pins[0.00] = Vector(0.10, 0.0, 0.90);
   pins[0.07] = Vector(0.00, 0.00, 1.00);
   pins[0.14] = Vector(0.63, 0.63, 1.00);
   pins[0.21] = Vector(0.00, 0.75, 1.00);
   pins[0.28] = Vector(0.00, 1.00, 1.00);
   pins[0.35] = Vector(0.10, 0.80, 0.70);
   pins[0.42] = Vector(0.10, 0.90, 0.00);
   pins[0.50] = Vector(0.50, 1.00, 0.63);
   pins[0.57] = Vector(0.75, 1.00, 0.25);
   pins[0.64] = Vector(1.00, 1.00, 0.00);
   pins[0.71] = Vector(1.00, 0.80, 0.10);
   pins[0.78] = Vector(1.00, 0.60, 0.30);
   pins[0.85] = Vector(1.00, 0.67, 0.95);
   pins[0.92] = Vector(1.00, 0.00, 0.50);
   pins[1.00] = Vector(1.00, 0.00, 0.00);
   transferFunctions[Star] = pins;
   pins.clear();

   pins[0.] = Vector(1, 1, 1);
   pins[1.] = Vector(0, 0, 1);
   transferFunctions[Blue_Light] = pins;
   pins.clear();

   pins[0.00] = Vector(0, 0, 1);
   pins[0.25] = Vector(0, 1, 1);
   pins[0.50] = Vector(0, 1, 0);
   pins[0.75] = Vector(1, 1, 0);
   pins[1.00] = Vector(1, 0, 0);
   transferFunctions[ANSYS] = pins;
   pins.clear();

   pins[0.00] = Vector(0.231, 0.298, 0.752);
   pins[0.25] = Vector(0.552, 0.690, 0.996);
   pins[0.50] = Vector(0.866, 0.866, 0.866);
   pins[0.75] = Vector(0.956, 0.603, 0.486);
   pins[1.00] = Vector(0.705, 0.015, 0.149);
   transferFunctions[CoolWarm] = pins;
   pins.clear();

   pins[0.00] = Vector(0.00, 0.00, 0.35);
   pins[0.05] = Vector(0.00, 0.00, 1.00);
   pins[0.26] = Vector(0.00, 1.00, 1.00);
   pins[0.50] = Vector(0.00, 0.00, 1.00);
   pins[0.74] = Vector(1.00, 1.00, 1.00);
   pins[0.95] = Vector(1.00, 0.00, 0.00);
   pins[1.00] = Vector(0.40, 0.00, 0.00);
   transferFunctions[ITSM] = pins;
   pins.clear();

   pins[0.0] = Vector(0.40, 0.00, 0.40);
   pins[0.2] = Vector(0.00, 0.00, 1.00);
   pins[0.4] = Vector(0.00, 1.00, 1.00);
   pins[0.6] = Vector(0.00, 1.00, 0.00);
   pins[0.8] = Vector(1.00, 1.00, 0.00);
   pins[1.0] = Vector(1.00, 0.00, 0.00);
   transferFunctions[Rainbow] = pins;
   pins.clear();
}

Color::~Color() {

}

void Color::getMinMax(vistle::DataBase::const_ptr object,
                      vistle::Scalar & min, vistle::Scalar & max) {

   size_t numElements = object->getSize();

   if (Vec<Index>::const_ptr scal = Vec<Index>::as(object)) {
      const vistle::Index *x = &scal->x()[0];
#pragma omp parallel
      {
         Index tmin = std::numeric_limits<Index>::max();
         Index tmax = -std::numeric_limits<Index>::min();
#pragma omp for
         for (ssize_t index = 0; index < numElements; index ++) {
            if (x[index] < tmin)
               tmin = x[index];
            if (x[index] > tmax)
               tmax = x[index];
         }
#pragma omp critical
         {
            if (tmin < min)
               min = tmin;
            if (tmax > max)
               max = tmax;
         }
      }
   } else  if (Vec<Scalar>::const_ptr scal = Vec<Scalar>::as(object)) {
      const vistle::Scalar *x = &scal->x()[0];
#pragma omp parallel
      {
         Scalar tmin = std::numeric_limits<Scalar>::max();
         Scalar tmax = -std::numeric_limits<Scalar>::max();
#pragma omp for
         for (ssize_t index = 0; index < numElements; index ++) {
            if (x[index] < tmin)
               tmin = x[index];
            if (x[index] > tmax)
               tmax = x[index];
         }
#pragma omp critical
         {
            if (tmin < min)
               min = tmin;
            if (tmax > max)
               max = tmax;
         }
      }
   } else  if (Vec<Scalar,3>::const_ptr vec = Vec<Scalar,3>::as(object)) {
      const vistle::Scalar *x = &vec->x()[0];
      const vistle::Scalar *y = &vec->y()[0];
      const vistle::Scalar *z = &vec->z()[0];
#pragma omp parallel
      {
         Scalar tmin = std::numeric_limits<Scalar>::max();
         Scalar tmax = -std::numeric_limits<Scalar>::max();
#pragma omp for
         for (ssize_t index = 0; index < numElements; index ++) {
            Scalar v = Vector(x[index], y[index], z[index]).norm();
            if (v < tmin)
               tmin = v;
            if (v > tmax)
               tmax = v;
         }
#pragma omp critical
         {
            if (tmin < min)
               min = tmin;
            if (tmax > max)
               max = tmax;
         }
      }
   }
}

vistle::Texture1D::ptr Color::addTexture(vistle::DataBase::const_ptr object,
      const vistle::Scalar min, const vistle::Scalar max,
      const ColorMap & cmap) {

   const Scalar invRange = 1.f / (max - min);

   vistle::Texture1D::ptr tex(new vistle::Texture1D(cmap.width, min, max));
   unsigned char *pix = &tex->pixels()[0];
   for (size_t index = 0; index < cmap.width * 4; index ++)
      pix[index] = cmap.data[index];

   const size_t numElem = object->getSize();
   tex->coords().resize(numElem);
   auto tc = tex->coords().data();

   if (Vec<Scalar>::const_ptr f = Vec<Scalar>::as(object)) {

      const vistle::Scalar *x = &f->x()[0];

#pragma omp parallel for
      for (ssize_t index = 0; index < numElem; index ++)
         tc[index] = (x[index] - min) * invRange;
   } else if (Vec<Index>::const_ptr f = Vec<Index>::as(object)) {

      const vistle::Index *x = &f->x()[0];

#pragma omp parallel for
      for (ssize_t index = 0; index < numElem; index ++)
         tc[index] = (x[index] - min) * invRange;
   } else  if (Vec<Scalar,3>::const_ptr f = Vec<Scalar,3>::as(object)) {

      const vistle::Scalar *x = &f->x()[0];
      const vistle::Scalar *y = &f->y()[0];
      const vistle::Scalar *z = &f->z()[0];

#pragma omp parallel for
      for (ssize_t index = 0; index < numElem; index ++) {
         const Scalar v = Vector(x[index], y[index], z[index]).norm();
         tc[index] = (v - min) * invRange;
      }
   } else {
       std::cerr << "Color: cannot handle input of type " << object->getType() << std::endl;

#pragma omp parallel for
      for (ssize_t index = 0; index < numElem; index ++) {
         tc[index] = 0.;
      }
   }

   return tex;
}

bool Color::compute() {

   Coords::const_ptr coords = accept<Coords>("data_in");
   if (coords) {
      passThroughObject("data_out", coords);
   }
   DataBase::const_ptr data = accept<DataBase>("data_in");
   if (!data) {
      Object::const_ptr obj = accept<Object>("data_in");
      passThroughObject("data_out", obj);
      return true;
   }

   auto pins = transferFunctions[getIntParameter("map")];
   if (pins.empty()) {
       pins = transferFunctions[COVISE];
   }
   ColorMap cmap(pins, 32);

   Scalar min = std::numeric_limits<Scalar>::max();
   Scalar max = -std::numeric_limits<Scalar>::max();

   if (getFloatParameter("min") >= getFloatParameter("max")) {
      getMinMax(data, min, max);
   } else {
      min = getFloatParameter("min");
      max = getFloatParameter("max");
   }

   //std::cerr << "Color: [" << min << "--" << max << "]" << std::endl;

   auto out(addTexture(data, min, max, cmap));
   out->setGrid(data->grid());
   out->setMeta(data->meta());
   out->copyAttributes(data);

   addObject("data_out", out);

   return true;
}
