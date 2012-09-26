#include <iostream>
#include <iomanip>

#include <limits.h>

#include "message.h"
#include "messagequeue.h"
#include "object.h"

template<typename T>
static T min(T a, T b) { return a<b ? a : b; }

using namespace boost::interprocess;

namespace vistle {

template<int> size_t memorySize();

template<> size_t memorySize<4>() {

   return INT_MAX;
}

template<> size_t memorySize<8>() {

   //return 68719476736;
   return 1 << 28;
}

Shm* Shm::s_singleton = NULL;

Shm::Shm(const int m, const int r, const size_t size,
         message::MessageQueue * mq)
   : m_moduleID(m), m_rank(r), m_objectID(0), m_messageQueue(mq) {

   m_shm = new managed_shared_memory(open_or_create, "vistle", size);
}

Shm::~Shm() {

   delete m_shm;
}

Shm & Shm::instance(const int moduleID, const int rank,
                    message::MessageQueue * mq) {

   if (!s_singleton) {
      size_t memsize = memorySize<sizeof(void *)>();

      do {
         try {
            s_singleton = new Shm(moduleID, rank, memsize, mq);
         } catch (boost::interprocess::interprocess_exception ex) {
            memsize /= 2;
         }
      } while (!s_singleton && memsize >= 4096);

      if (!s_singleton) {
         std::cerr << "failed to allocate shared memory: module id: " << moduleID
            << ", rank: " << rank << ", message queue: " << (mq ? mq->getName() : "n/a") << std::endl;
      }
   }

   assert(s_singleton && "failed to allocate shared memory");

   return *s_singleton;
}

managed_shared_memory & Shm::getShm() {

   return *m_shm;
}

void Shm::publish(const shm_handle_t & handle) {

   if (m_messageQueue) {
      vistle::message::NewObject n(m_moduleID, m_rank, handle);
      m_messageQueue->getMessageQueue().send(&n, sizeof(n), 0);
   }
}

std::string Shm::createObjectID() {

   std::stringstream name;
   name << "Object_" << std::setw(8) << std::setfill('0') << m_moduleID
        << "_" << std::setw(8) << std::setfill('0') << m_rank
        << "_" << std::setw(8) << std::setfill('0') << m_objectID++;

   return name.str();
}

shm_handle_t Shm::getHandleFromObject(Object::const_ptr object) {

   try {
      return m_shm->get_handle_from_address(object->d());

   } catch (interprocess_exception &ex) { }

   return 0;
}

Object::const_ptr Shm::getObjectFromHandle(const shm_handle_t & handle) {

   try {
      Object::Data *od = static_cast<Object::Data *>
         (m_shm->get_address_from_handle(handle));

      return Object::create(od);
   } catch (interprocess_exception &ex) { }

   return Object::const_ptr();
}

Object::ptr Object::create(Object::Data *data) {

   if (!data)
      return Object::ptr();

#define CR(id, T) case id: return Object::ptr(new T(static_cast<T::Data *>(data)))

   switch(data->type) {
      case Object::UNKNOWN: assert(0 == "Cannot create Object of UNKNOWN type");
      CR(Object::VECCHAR, Vec<char>);
      CR(Object::VECINT, Vec<int>);
      CR(Object::VECFLOAT, Vec<Scalar>);
      CR(Object::VEC3CHAR, Vec3<char>);
      CR(Object::VEC3INT, Vec3<int>);
      CR(Object::VEC3FLOAT, Vec3<Scalar>);
      CR(Object::LINES, Lines);
      CR(Object::TRIANGLES, Triangles);
      CR(Object::POLYGONS, Polygons);
      CR(Object::UNSTRUCTUREDGRID, UnstructuredGrid);
      CR(Object::TEXTURE1D, Texture1D);
      CR(Object::GEOMETRY, Geometry);
      CR(Object::SET, Set);
   }

#undef CR

   std::cerr << "Attempt to create object of invalid type " << data->type << std::endl;

   assert(0 == "Cannot create Object of invalid type");
   return Object::ptr();
}

Object::Data::Data(const Type type, const std::string & n,
               const int b, const int t): type(type), refcount(0), block(b), timestep(t) {

   size_t size = min(n.size(), sizeof(name)-1);
   n.copy(name, size);
   name[size] = 0;
}

Object::Object(Object::Data *data)
: m_data(data)
{
   m_data->ref();
}

Object::~Object() {

   assert(m_data == NULL && "should have been deleted by a derived class");
}

Object::Info *Object::getInfo(Object::Info *info) const {

   if (!info)
      info = new Info;

   info->infosize = sizeof(Info);
   info->itemsize = 0;
   info->offset = 0;
   info->type = getType();
   info->block = getBlock();
   info->timestep = getTimestep();

   return info;
}

Object::Type Object::getType() const {

   return d()->type;
}

std::string Object::getName() const {

   return d()->name;
}

int Object::getTimestep() const {

   return d()->timestep;
}

int Object::getBlock() const {

   return d()->block;
}

void Object::setTimestep(const int time) {

   d()->timestep = time;
}

void Object::setBlock(const int blk) {

   d()->block = blk;
}

Coords::Data::Data(const size_t numVertices,
      Type id, const std::string &name,
      const int block, const int timestep)
   : Coords::Base::Data(numVertices,
         id, name,
         block, timestep)
{
}

Coords::Info *Coords::getInfo(Coords::Info *info) const {

   if (!info)
      info = new Info;

   Base::getInfo(info);

   info->infosize = sizeof(Info);
   info->itemsize = 0;
   info->offset = 0;
   info->numVertices = getNumVertices();

   return info;
}

size_t Coords::getNumVertices() const {

   return getSize();
}

Triangles::Triangles(const size_t numCorners, const size_t numVertices,
                     const int block, const int timestep)
   : Triangles::Base(Triangles::Data::create(numCorners, numVertices,
            block, timestep)) {
}

Triangles::Data::Data(const size_t numCorners, const size_t numVertices,
                     const std::string & name,
                     const int block, const int timestep)
   : Base::Data(numCorners,
         Object::TRIANGLES, name,
         block, timestep)
   , cl(shm<size_t>::construct_vector(numCorners))
{
}


Triangles::Data * Triangles::Data::create(const size_t numCorners,
                              const size_t numVertices,
                              const int block, const int timestep) {

   const std::string name = Shm::instance().createObjectID();
   Data *t = shm<Data>::construct(name)(numCorners, numVertices, name, block, timestep);

   /*
   shm_handle_t handle =
      Shm::instance().getShm().get_handle_from_address(t);
   Shm::instance().publish(handle);
   */
   return t;
}

Triangles::Info *Triangles::getInfo(Triangles::Info *info) const {

   if (!info)
      info = new Info;

   Base::getInfo(info);

   info->infosize = sizeof(Info);
   info->itemsize = 0;
   info->offset = 0;
   info->numCorners = getNumCorners();
   info->numVertices = getNumVertices();

   return info;
}

size_t Triangles::getNumCorners() const {

   return d()->cl->size();
}

size_t Triangles::getNumVertices() const {

   return d()->x->size();
}

Indexed::Info *Indexed::getInfo(Indexed::Info *info) const {

   if (!info)
      info = new Info;

   Base::getInfo(info);

   info->infosize = sizeof(Info);
   info->itemsize = 0;
   info->offset = 0;
   info->numVertices = getNumVertices();
   info->numElements = getNumElements();
   info->numCorners = getNumCorners();

   return info;
}

Indexed::Data::Data(const size_t numElements, const size_t numCorners,
             const size_t numVertices,
             Type id, const std::string & name,
             const int block, const int timestep)
   : Indexed::Base::Data(numVertices, id, name,
         block, timestep)
   , el(shm<size_t>::construct_vector(numElements))
   , cl(shm<size_t>::construct_vector(numCorners))
{
}

Lines::Lines(const size_t numElements, const size_t numCorners,
                      const size_t numVertices,
                      const int block, const int timestep)
   : Lines::Base(Lines::Data::create(numElements, numCorners,
            numVertices, block, timestep))
{
}

Lines::Data::Data(const size_t numElements, const size_t numCorners,
             const size_t numVertices, const std::string & name,
             const int block, const int timestep)
   : Lines::Base::Data(numElements, numCorners, numVertices,
         Object::LINES, name, block, timestep)
{
}


Lines::Data * Lines::Data::create(const size_t numElements, const size_t numCorners,
                      const size_t numVertices,
                      const int block, const int timestep) {

   const std::string name = Shm::instance().createObjectID();
   Data *l = shm<Data>::construct(name)(numElements, numCorners, numVertices, name, block, timestep);

   /*
   shm_handle_t handle =
      Shm::instance().getShm().get_handle_from_address(l);
   Shm::instance().publish(handle);
   */
   return l;
}

Lines::Info *Lines::getInfo(Lines::Info *info) const {

   if (!info)
      info = new Info;

   Base::getInfo(info);

   info->infosize = sizeof(Info);
   info->itemsize += info->numElements * sizeof(uint64_t)
      + info->numCorners * sizeof(uint64_t)
      + 3 * info->numVertices * sizeof(Scalar);
   info->offset = 0;
   info->numElements = getNumElements();
   info->numCorners = getNumCorners();
   info->numVertices = getNumVertices();

   return info;
}

size_t Indexed::getNumElements() const {

   return d()->el->size();
}

size_t Indexed::getNumCorners() const {

   return d()->cl->size();
}

size_t Indexed::getNumVertices() const {

   return d()->x->size();
}

Polygons::Polygons(const size_t numElements,
      const size_t numCorners,
      const size_t numVertices,
      const int block, const int timestep)
: Polygons::Base(Polygons::Data::create(numElements, numCorners, numVertices, block, timestep))
{
}

Polygons::Data::Data(const size_t numElements, const size_t numCorners,
                   const size_t numVertices, const std::string & name,
                   const int block, const int timestep)
   : Polygons::Base::Data(numElements, numCorners, numVertices,
         Object::POLYGONS, name, block, timestep)
{
}


Polygons::Data * Polygons::Data::create(const size_t numElements,
                            const size_t numCorners,
                            const size_t numVertices,
                            const int block, const int timestep) {

   const std::string name = Shm::instance().createObjectID();
   Data *p = shm<Data>::construct(name)(numElements, numCorners, numVertices, name, block, timestep);

   /*
   shm_handle_t handle =
      Shm::instance().getShm().get_handle_from_address(p);
   Shm::instance().publish(handle);
   */
   return p;
}

UnstructuredGrid::UnstructuredGrid(const size_t numElements,
      const size_t numCorners,
      const size_t numVertices,
      const int block, const int timestep)
   : UnstructuredGrid::Base(UnstructuredGrid::Data::create(numElements, numCorners, numVertices, block, timestep))
{
}

UnstructuredGrid::Data::Data(const size_t numElements,
                                   const size_t numCorners,
                                   const size_t numVertices,
                                   const std::string & name,
                                   const int block, const int timestep)
   : Base::Data(Object::UNSTRUCTUREDGRID, name, block, timestep)
   , tl(shm<char>::construct_vector(numElements))
   , el(shm<size_t>::construct_vector(numElements))
   , cl(shm<size_t>::construct_vector(numCorners))
   , x(shm<Scalar>::construct_vector(numVertices))
   , y(shm<Scalar>::construct_vector(numVertices))
   , z(shm<Scalar>::construct_vector(numVertices))
{
}

UnstructuredGrid::Data * UnstructuredGrid::Data::create(const size_t numElements,
                                            const size_t numCorners,
                                            const size_t numVertices,
                                            const int block, const int timestep) {

   const std::string name = Shm::instance().createObjectID();
   Data *u = shm<Data>::construct(name)(numElements, numCorners, numVertices, name, block, timestep);

   /*
   shm_handle_t handle =
      Shm::instance().getShm().get_handle_from_address(u);
   Shm::instance().publish(handle);
   */
   return u;
}

size_t UnstructuredGrid::getNumElements() const {

   return d()->el->size();
}

size_t UnstructuredGrid::getNumCorners() const {

   return d()->cl->size();
}

size_t UnstructuredGrid::getNumVertices() const {

   return d()->x->size();
}


Set::Set(const size_t numElements,
                  const int block, const int timestep)
: Set::Base(Set::Data::create(numElements, block, timestep))
{
}

Set::Data::Data(const size_t numElements, const std::string & name,
         const int block, const int timestep)
   : Set::Base::Data(Object::SET, name, block, timestep)
     , elements(shm<offset_ptr<Object::Data> >::construct_vector(numElements))
{
}


Set::Data * Set::Data::Data::create(const size_t numElements,
                  const int block, const int timestep) {

   const std::string name = Shm::instance().createObjectID();
   Data *p = shm<Data>::construct(name)(numElements, name, block, timestep);

   /*
   shm_handle_t handle =
      Shm::instance().getShm().get_handle_from_address(p);
   Shm::instance().publish(handle);
   */
   return p;
}

size_t Set::getNumElements() const {

   return d()->elements->size();
}

Object::const_ptr Set::getElement(const size_t index) const {

   if (index >= d()->elements->size())
      return Object::ptr();

   return Object::create((*d()->elements)[index].get());
}

void Set::setElement(const size_t index, Object::const_ptr object) {

   if (index >= d()->elements->size()) {

      std::cerr << "WARNING: automatically resisizing set" << std::endl;
      d()->elements->resize(index+1);
   }

   if (object)
      (*d()->elements)[index] = object->d();
   else
      (*d()->elements)[index] = NULL;
}

void Set::addElement(Object::const_ptr object) {

   if (object)
      d()->elements->push_back(object->d());
}

Geometry::Geometry(const int block, const int timestep)
   : Geometry::Base(Geometry::Data::create(block, timestep))
{
}

Geometry::Data::Data(const std::string & name,
      const int block, const int timestep)
   : Geometry::Base::Data(Object::GEOMETRY, name, block, timestep)
   , geometry(NULL)
   , colors(NULL)
   , normals(NULL)
   , texture(NULL)
{
}


Geometry::Data * Geometry::Data::create(const int block, const int timestep) {

   const std::string name = Shm::instance().createObjectID();
   Data *g = shm<Data>::construct(name)(name, block, timestep);

   /*
   shm_handle_t handle =
      Shm::instance().getShm().get_handle_from_address(g);
   Shm::instance().publish(handle);
   */
   return g;
}

void Geometry::setGeometry(Object::const_ptr g) {

   d()->geometry = g->d();
}

void Geometry::setColors(Object::const_ptr c) {

   d()->colors = c->d();
}

void Geometry::setTexture(Object::const_ptr t) {

   d()->texture = t->d();
}

void Geometry::setNormals(Object::const_ptr n) {

   d()->normals = n->d();
}

Object::const_ptr Geometry::geometry() const {

   return Object::create(&*d()->geometry);
}

Object::const_ptr Geometry::colors() const {

   return Object::create(&*d()->colors);
}

Object::const_ptr Geometry::normals() const {

   return Object::create(&*d()->normals);
}

Object::const_ptr Geometry::texture() const {

   return Object::create(&*d()->texture);
}

Texture1D::Texture1D(const size_t width,
      const Scalar min, const Scalar max,
      const int block, const int timestep)
: Texture1D::Base(Texture1D::Data::create(width, min, max, block, timestep))
{
}

Texture1D::Data::Data(const std::string &name, const size_t width,
                     const Scalar mi, const Scalar ma,
                     const int block, const int timestep)
   : Texture1D::Base::Data(Object::TEXTURE1D, name, block, timestep)
   , min(mi)
   , max(ma)
   , pixels(shm<unsigned char>::construct_vector(width * 4))
   , coords(shm<Scalar>::construct_vector(1))
{

#if  0
   const allocator<Scalar, managed_shared_memory::segment_manager>
      alloc_inst_Scalar(Shm::instance().getShm().get_segment_manager());

   coords = Shm::instance().getShm().construct<shm<Scalar>::vector>(Shm::instance().createObjectID().c_str())(alloc_inst_Scalar);
#endif
}

Texture1D::Data *Texture1D::Data::create(const size_t width,
                              const Scalar min, const Scalar max,
                              const int block, const int timestep) {

   const std::string name = Shm::instance().createObjectID();
   Data *tex= shm<Data>::construct(name)(name, width, min, max, block, timestep);

   /*
   shm_handle_t handle =
      Shm::instance().getShm().get_handle_from_address(tex);
   Shm::instance().publish(handle);
   */
   return tex;
}

size_t Texture1D::getNumElements() const {

   return d()->coords->size();
}

size_t Texture1D::getWidth() const {

   return d()->pixels->size() / 4;
}

template<> const Object::Type Vec<Scalar>::s_type  = Object::VECFLOAT;
template<> const Object::Type Vec<int>::s_type    = Object::VECINT;
template<> const Object::Type Vec<char>::s_type   = Object::VECCHAR;
template<> const Object::Type Vec3<Scalar>::s_type = Object::VEC3FLOAT;
template<> const Object::Type Vec3<int>::s_type   = Object::VEC3INT;
template<> const Object::Type Vec3<char>::s_type  = Object::VEC3CHAR;

} // namespace vistle
