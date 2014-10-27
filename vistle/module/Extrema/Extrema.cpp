#include <sstream>
#include <limits>
#include <algorithm>

#include <boost/mpl/for_each.hpp>
#include <boost/mpi/collectives/all_reduce.hpp>

#include <core/vec.h>
#include <module/module.h>
#include <core/scalars.h>
#include <core/paramvector.h>
#include <core/message.h>
#include <core/coords.h>
#include <core/lines.h>

using namespace vistle;

class Extrema: public vistle::Module {

 public:
   Extrema(const std::string &shmname, int rank, int size, int moduleID);
   ~Extrema();

 private:
   static const int MaxDim = ParamVector::MaxDimension;

   int dim;
   bool handled;
   bool haveGeometry;
   ParamVector min, max, gmin, gmax;
   IntParamVector minIndex, maxIndex, gminIndex, gmaxIndex;
   IntParamVector minBlock, maxBlock, gminBlock, gmaxBlock;

   virtual bool compute();
   virtual bool reduce(int timestep);

   template<int Dim>
   friend struct Compute;

   bool prepare() {
      haveGeometry = false;

      for (int c=0; c<MaxDim; ++c) {
         gmin[c] =  std::numeric_limits<ParamVector::Scalar>::max();
         gmax[c] = -std::numeric_limits<ParamVector::Scalar>::max();
         gminIndex[c] = -1;
         gmaxIndex[c] = -1;
      }

      return true;
   }

   template<int Dim>
   struct Compute {
      Object::const_ptr object;
      Extrema *module;
      Compute(Object::const_ptr obj, Extrema *module)
         : object(obj)
         , module(module)
      {
      }
      template<typename S> void operator()(S) {

         typedef Vec<S,Dim> V;
         typename V::const_ptr in(V::as(object));
         if (!in)
            return;

         module->handled = true;
         module->min.dim = Dim;
         module->max.dim = Dim;
         module->minIndex.dim = Dim;
         module->maxIndex.dim = Dim;
         module->minBlock.dim = Dim;
         module->maxBlock.dim = Dim;

         size_t size = in->getSize();
#pragma omp parallel
         for (int c=0; c<Dim; ++c) {
            S min = module->min[c];
            S max = module->max[c];
            const S *x = in->x(c).data();
            Index imin=InvalidIndex, imax=InvalidIndex;
            for (unsigned int index = 0; index < size; index ++) {
               if (x[index] < min) {
                  min = x[index];
                  imin = index;
               }
               if (x[index] > max) {
                  max = x[index];
                  imax = index;
               }
            }
            if (imin != InvalidIndex) {
               module->min[c] = min;
               module->minIndex[c] = imin;
               module->minBlock[c] = in->getBlock();
            }
            if (imax != InvalidIndex) {
               module->max[c] = max;
               module->maxIndex[c] = imax;
               module->maxBlock[c] = in->getBlock();
            }
         }
      }
   };

};

using namespace vistle;

Extrema::Extrema(const std::string &shmname, int rank, int size, int moduleID)
   : Module("Extrema", shmname, rank, size, moduleID)
{

   setReducePolicy(message::ReducePolicy::OverAll);

   Port *din = createInputPort("data_in", "input data", Port::MULTI);
   createOutputPort("grid_out", "bounding box", Port::MULTI);
   Port *dout = createOutputPort("data_out", "output data", Port::MULTI);
   din->link(dout);

   addVectorParameter("min",
         "output parameter: minimum",
         ParamVector(
            std::numeric_limits<ParamVector::Scalar>::max(),
            std::numeric_limits<ParamVector::Scalar>::max(),
            std::numeric_limits<ParamVector::Scalar>::max()
            ));
   addVectorParameter("max",
         "output parameter: maximum",
         ParamVector(
            -std::numeric_limits<ParamVector::Scalar>::max(),
            -std::numeric_limits<ParamVector::Scalar>::max(),
            -std::numeric_limits<ParamVector::Scalar>::max()
            ));
   addIntVectorParameter("min_block",
         "output parameter: block numbers containing minimum",
         IntParamVector(-1, -1, -1));
   addIntVectorParameter("max_block",
         "output parameter: block numbers containing maximum",
         IntParamVector(-1, -1, -1));
   addIntVectorParameter("min_index",
         "output parameter: indices of minimum",
         IntParamVector(-1, -1, -1));
   addIntVectorParameter("max_index",
         "output parameter: indices of maximum",
         IntParamVector(-1, -1, -1));
}

Extrema::~Extrema() {

}

bool Extrema::compute() {

   //std::cerr << "Extrema: compute: execcount=" << m_executionCount << std::endl;

   dim = -1;

   while(Object::const_ptr obj = takeFirstObject("data_in")) {
      handled = false;

      for (int c=0; c<MaxDim; ++c) {
         min[c] =  std::numeric_limits<ParamVector::Scalar>::max();
         max[c] = -std::numeric_limits<ParamVector::Scalar>::max();
      }

      boost::mpl::for_each<Scalars>(Compute<1>(obj, this));
      boost::mpl::for_each<Scalars>(Compute<3>(obj, this));

      if (!handled) {

         std::string error("could not handle input");
         std::cerr << "Extrema: " << error << std::endl;
         throw(vistle::exception(error));
      }

      if (dim == -1) {
         dim = min.dim;
         gmin.dim = dim;
         gmax.dim = dim;
         gminIndex.dim = dim;
         gmaxIndex.dim = dim;
         gminBlock.dim = dim;
         gmaxBlock.dim = dim;
      } else if (dim != min.dim) {
         std::string error("input dimensions not equal");
         std::cerr << "Extrema: " << error << std::endl;
         throw(vistle::exception(error));
      }

      Object::ptr out = obj->clone();
      out->addAttribute("min", min.str());
      out->addAttribute("max", max.str());
      out->addAttribute("minIndex", minIndex.str());
      out->addAttribute("maxIndex", maxIndex.str());
      //std::cerr << "Extrema: min " << min << ", max " << max << std::endl;

      addObject("data_out", out);

      for (int c=0; c<MaxDim; ++c) {
         if (gmin[c] > min[c]) {
            gmin[c] = min[c];
            gminIndex[c] = minIndex[c];
            gminBlock[c] = minBlock[c];
         }
         if (gmax[c] < max[c]) {
            gmax[c] = max[c];
            gmaxIndex[c] = maxIndex[c];
            gmaxBlock[c] = maxBlock[c];
         }
      }

      if (Coords::as(obj)) {
         haveGeometry = true;
      }
   }

   return true;
}

bool Extrema::reduce(int timestep) {

   //std::cerr << "reduction for timestep " << timestep << std::endl;

   for (int i=0; i<MaxDim; ++i) {
      gmin[i] = boost::mpi::all_reduce(boost::mpi::communicator(),
            gmin[i],
            boost::mpi::minimum<ParamVector::Scalar>());
      gmax[i] = boost::mpi::all_reduce(boost::mpi::communicator(),
            gmax[i],
            boost::mpi::maximum<ParamVector::Scalar>());
   }

   setVectorParameter("min", gmin);
   setVectorParameter("max", gmax);
   setIntVectorParameter("min_block", gminBlock);
   setIntVectorParameter("max_block", gmaxBlock);
   setIntVectorParameter("min_index", gminIndex);
   setIntVectorParameter("max_index", gmaxIndex);

   if (haveGeometry && rank() == 0) {

      Lines::ptr box(new Lines(4, 16, 8));
      Scalar *x[3];
      for (int i=0; i<3; ++i) {
         x[i] = box->x(i).data();
      }
      auto corners = box->cl().data();
      auto elements = box->el().data();
      for (int i=0; i<=4; ++i) { // include sentinel
         elements[i] = 4*i;
      }
      corners[0] = 0;
      corners[1] = 1;
      corners[2] = 3;
      corners[3] = 2;

      corners[4] = 1;
      corners[5] = 5;
      corners[6] = 7;
      corners[7] = 3;

      corners[8] = 5;
      corners[9] = 4;
      corners[10] = 6;
      corners[11] = 7;

      corners[12] = 4;
      corners[13] = 0;
      corners[14] = 2;
      corners[15] = 6;

      for (int c=0; c<3; ++c) {
         int p = 1 << c;
         for (int i=0; i<8; ++i) {
            if (i & p)
               x[c][i] = gmax[c];
            else
               x[c][i] = gmin[c];

         }
      }

      addObject("grid_out", box);
   }

   return Module::reduce(timestep);
}

MODULE_MAIN(Extrema)

