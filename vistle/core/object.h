#ifndef OBJECT_H
#define OBJECT_H

#include <vector>
#include <util/sysdep.h>
#include <memory>
#include <iostream>
#include <string>
#include <atomic>

#ifdef NO_SHMEM
#include <map>
#else
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#endif
#include <boost/interprocess/exceptions.hpp>

#include <boost/mpl/size.hpp>

#include <util/enum.h>

#include "export.h"
#include "shmname.h"
#include "shm_array.h"
#include "objectmeta.h"
#include "scalars.h"
#include "dimensions.h"
#include "vector.h"

#include "archives_config.h"


namespace vistle {

namespace interprocess = boost::interprocess;

#ifdef NO_SHMEM
#else
typedef interprocess::managed_shared_memory::handle_t shm_handle_t;
#endif

class Shm;

struct ObjectData;
class Object;


class V_COREEXPORT ObjectInterfaceBase {
public:
    //std::shared_ptr<const Object> object() const { static_cast<const Object *>(this)->shared_from_this(); }
    virtual std::shared_ptr<const Object> object() const = 0;
protected:
    virtual ~ObjectInterfaceBase() {}
};

class V_COREEXPORT Object: public std::enable_shared_from_this<Object>, virtual public ObjectInterfaceBase {
   friend class Shm;
   friend class ObjectTypeRegistry;
   friend struct ObjectData;
   template<class ObjType>
   friend class shm_obj_ref;
   friend void Shm::markAsRemoved(const std::string &name);

public:
   typedef std::shared_ptr<Object> ptr;
   typedef std::shared_ptr<const Object> const_ptr;
   typedef ObjectData Data;

   template<class Interface>
   const Interface *getInterface() const {
       return dynamic_cast<const Interface *>(this);
   }

   std::shared_ptr<const Object> object() const override { return static_cast<const Object *>(this)->shared_from_this(); }

   enum InitializedFlags {
      Initialized
   };

   enum Type {
      COORD             = -9,
      COORDWRADIUS      = -8,
      DATABASE          = -7,

      UNKNOWN           = -1,

      PLACEHOLDER       = 11,

      TEXTURE1D         = 16,

      POINTS            = 18,
      SPHERES           = 19,
      LINES             = 20,
      TUBES             = 21,
      TRIANGLES         = 22,
      POLYGONS          = 23,
      UNSTRUCTUREDGRID  = 24,
      UNIFORMGRID       = 25,
      RECTILINEARGRID   = 26,
      STRUCTUREDGRID    = 27,

      VERTEXOWNERLIST   = 95,
      CELLTREE1         = 96,
      CELLTREE2         = 97,
      CELLTREE3         = 98,
      NORMALS           = 99,

      VEC               = 100, // base type id for all Vec types
   };

   static const char *toString(Type v);

   virtual ~Object();

   bool operator==(const Object &other) const;
   bool operator!=(const Object &other) const;

   bool isComplete() const; //! check whether all references have been resolved

   static const char *typeName() { return "Object"; }
   Object::ptr clone() const;
   virtual Object::ptr cloneInternal() const = 0;

   Object::ptr cloneType() const;
   virtual Object::ptr cloneTypeInternal() const = 0;

   static Object *createEmpty(const std::string &name = std::string());

   virtual void refresh() const; //!< refresh cached pointers from shm
   virtual bool check() const;
   virtual void updateInternals();

   virtual bool isEmpty() const;

   template<class ObjectType>
   static std::shared_ptr<const Object> as(std::shared_ptr<const ObjectType> ptr) { return std::static_pointer_cast<const Object>(ptr); }
   template<class ObjectType>
   static std::shared_ptr<Object> as(std::shared_ptr<ObjectType> ptr) { return std::static_pointer_cast<Object>(ptr); }

   shm_handle_t getHandle() const;

   Type getType() const;
   std::string getName() const;

   int getBlock() const;
   int getNumBlocks() const;
   double getRealTime() const;
   int getTimestep() const;
   int getNumTimesteps() const;
   int getIteration() const;
   int getExecutionCounter() const;
   int getCreator() const;
   Matrix4 getTransform() const;

   void setBlock(const int block);
   void setNumBlocks(const int num);
   void setRealTime(double time);
   void setTimestep(const int timestep);
   void setNumTimesteps(const int num);
   void setIteration(const int num);
   void setExecutionCounter(const int count);
   void setCreator(const int id);
   void setTransform(const Matrix4 &transform);

   const Meta &meta() const;
   void setMeta(const Meta &meta);

   void addAttribute(const std::string &key, const std::string &value = "");
   void setAttributeList(const std::string &key, const std::vector<std::string> &values);
   virtual void copyAttributes(Object::const_ptr src, bool replace = true);
   bool hasAttribute(const std::string &key) const;
   std::string getAttribute(const std::string &key) const;
   std::vector<std::string> getAttributes(const std::string &key) const;
   std::vector<std::string> getAttributeList() const;

   // attachments, e.g. Celltrees
   bool addAttachment(const std::string &key, Object::const_ptr att) const;
   bool hasAttachment(const std::string &key) const;
   void copyAttachments(Object::const_ptr src, bool replace = true);
   Object::const_ptr getAttachment(const std::string &key) const;
   bool removeAttachment(const std::string &key) const;

   void ref() const;
   void unref() const;
   int refcount() const;

   template<class Archive>
   static Object *loadObject(Archive &ar);

   template<class Archive>
   void saveObject(Archive &ar) const;

   ARCHIVE_REGISTRATION(=0)

#ifdef USE_INTROSPECTION_ARCHIVE
   virtual void save(FindObjectReferenceOArchive &ar) const = 0;
#endif

 public:

 protected:
   Data *m_data;
 public:
   Data *d() const { return m_data; }
 protected:

   Object(Data *data);
   Object();

   static void publish(const Data *d);
   static Object *create(Data *);
 private:
   std::string m_name; // just a debugging aid
   template<class Archive>
   void serialize(Archive &ar);
   ARCHIVE_ACCESS

   // not implemented
   Object(const Object &) = delete;
   Object &operator=(const Object &) = delete;
};

ARCHIVE_ASSUME_ABSTRACT(Object)

struct ObjectData {
    Object::Type type;
    shm_name_t name;
    mutable std::atomic<int> refcount;

    int unresolvedReferences; //!< no. of not-yet-available arrays and referenced objects

    Meta meta;

    typedef shm<char>::string Attribute;
    typedef shm<char>::string Key;
    typedef shm<Attribute>::vector AttributeList;
    typedef std::pair<const Key, AttributeList> AttributeMapValueType;
    typedef shm<AttributeMapValueType>::allocator AttributeMapAllocator;
#ifdef NO_SHMEM
    typedef std::map<Key, AttributeList, std::less<Key>, AttributeMapAllocator> AttributeMap;
#else
    typedef interprocess::map<Key, AttributeList, std::less<Key>, AttributeMapAllocator> AttributeMap;
#endif
    AttributeMap attributes;
    typedef std::map<std::string, std::vector<std::string>> StdAttributeMap; // for serialization
    void addAttribute(const std::string &key, const std::string &value = "");
    V_COREEXPORT void setAttributeList(const std::string &key, const std::vector<std::string> &values);
    void copyAttributes(const ObjectData *src, bool replace);
    bool hasAttribute(const std::string &key) const;
    std::string getAttribute(const std::string &key) const;
    V_COREEXPORT std::vector<std::string> getAttributes(const std::string &key) const;
    V_COREEXPORT std::vector<std::string> getAttributeList() const;

#ifdef NO_SHMEM
    mutable std::recursive_mutex attachment_mutex;
    typedef std::lock_guard<std::recursive_mutex> attachment_mutex_lock_type;
    typedef const ObjectData *Attachment;
#else
    mutable boost::interprocess::interprocess_recursive_mutex attachment_mutex; //< protects attachments
    typedef boost::interprocess::scoped_lock<boost::interprocess::interprocess_recursive_mutex> attachment_mutex_lock_type;
    typedef interprocess::offset_ptr<const ObjectData> Attachment;
#endif
    typedef std::pair<const Key, Attachment> AttachmentMapValueType;
    typedef shm<AttachmentMapValueType>::allocator AttachmentMapAllocator;
#ifdef NO_SHMEM
    typedef std::map<Key, Attachment, std::less<Key>, AttachmentMapAllocator> AttachmentMap;
#else
    typedef interprocess::map<Key, Attachment, std::less<Key>, AttachmentMapAllocator> AttachmentMap;
#endif
    AttachmentMap attachments;
    bool addAttachment(const std::string &key, Object::const_ptr att);
    void copyAttachments(const ObjectData *src, bool replace);
    bool hasAttachment(const std::string &key) const;
    Object::const_ptr getAttachment(const std::string &key) const;
    bool removeAttachment(const std::string &key);

    V_COREEXPORT ObjectData(Object::Type id = Object::UNKNOWN, const std::string &name = "", const Meta &m=Meta());
    V_COREEXPORT ObjectData(const ObjectData &other, const std::string &name, Object::Type id=Object::UNKNOWN); //! shallow copy, except for attributes
    V_COREEXPORT ~ObjectData();
#ifndef NO_SHMEM
    V_COREEXPORT void *operator new(size_t size);
    V_COREEXPORT void *operator new (std::size_t size, void* ptr);
    V_COREEXPORT void operator delete(void *ptr);
    V_COREEXPORT void operator delete(void *ptr, void* voidptr2);
#endif
    V_COREEXPORT void ref() const;
    V_COREEXPORT void unref() const;
    static ObjectData *create(Object::Type id, const std::string &name, const Meta &m);
    V_COREEXPORT bool isComplete() const; //! check whether all references have been resolved
    V_COREEXPORT void unresolvedReference();
    V_COREEXPORT void referenceResolved(const std::function<void()> &completeCallback);

    ARCHIVE_ACCESS_SPLIT
    template<class Archive>
    void save(Archive &ar) const;
    template<class Archive>
    void load(Archive &ar);

    // not implemented
    ObjectData(const ObjectData &) = delete;
    ObjectData &operator=(const ObjectData &) = delete;
};


class V_COREEXPORT ObjectTypeRegistry {
public:
   typedef Object *(*CreateEmptyFunc)(const std::string &name);
   typedef Object *(*CreateFunc)(Object::Data *d);
   typedef void (*DestroyFunc)(const std::string &name);

   struct FunctionTable {
       CreateEmptyFunc createEmpty;
       CreateFunc create;
       DestroyFunc destroy;
   };

   template<class O>
   static void registerType(int id) {
#ifndef VISTLE_STATIC
      assert(typeMap().find(id) == typeMap().end());
#endif
      struct FunctionTable t = {
         O::createEmpty,
         O::createFromData,
         O::destroy,
      };
      typeMap()[id] = t;
   }

   static const struct FunctionTable &getType(int id);

   static CreateFunc getCreator(int id);
   static DestroyFunc getDestroyer(int id);

private:
   typedef std::map<int, FunctionTable> TypeMap;

   static TypeMap &typeMap();
};

//! use in checkImpl
#define V_CHECK(true_expr) \
   if (!(true_expr)) { \
      std::cerr << __FILE__ << ":" << __LINE__ << ": " \
      << "CONSISTENCY CHECK FAILURE on " << this->getName() << " " << this->meta() << ": " \
          << #true_expr << std::endl; \
      std::cerr << "   PID: " << getpid() << std::endl; \
      std::stringstream str; \
      str << __FILE__ << ":" << __LINE__ << ": consistency check failure: " << #true_expr; \
      throw(vistle::except::consistency_error(str.str())); \
      sleep(30); \
      return false; \
   }

//! declare a new Object type
#define V_OBJECT(ObjType) \
   public: \
   typedef std::shared_ptr<ObjType> ptr; \
   typedef std::shared_ptr<const ObjType> const_ptr; \
   typedef ObjType Class; \
   static Object::Type type(); \
   static const char *typeName() { return #ObjType; } \
   static std::shared_ptr<const ObjType> as(std::shared_ptr<const Object> ptr) { return std::dynamic_pointer_cast<const ObjType>(ptr); } \
   static std::shared_ptr<ObjType> as(std::shared_ptr<Object> ptr) { return std::dynamic_pointer_cast<ObjType>(ptr); } \
   static Object *createFromData(Object::Data *data) { return new ObjType(static_cast<ObjType::Data *>(data)); } \
   std::shared_ptr<const Object> object() const override { return static_cast<const Object *>(this)->shared_from_this(); } \
   Object::ptr cloneInternal() const override { \
      const std::string n(Shm::the().createObjectId()); \
            Data *data = shm<Data>::construct(n)(*d(), n); \
            publish(data); \
            return Object::ptr(createFromData(data)); \
   } \
   ptr clone() const { \
      return ObjType::as(cloneInternal()); \
   } \
   ptr cloneType() const { \
      return ObjType::as(cloneTypeInternal()); \
   } \
   Object::ptr cloneTypeInternal() const override { \
      return Object::ptr(new ObjType(Object::Initialized)); \
   } \
   static Object *createEmpty(const std::string &name) { \
      if (name.empty()) \
          return new ObjType(Object::Initialized); \
      auto d = Data::createNamed(ObjType::type(), name); \
      return new ObjType(d); \
   } \
   template<class OtherType> \
   static ptr clone(typename OtherType::ptr other) { \
      const std::string n(Shm::the().createObjectId()); \
      typename ObjType::Data *data = shm<typename ObjType::Data>::construct(n)(*other->d(), n); \
      assert(data->type == ObjType::type()); \
      ObjType *ret = dynamic_cast<ObjType *>(createFromData(data)); \
      assert(ret); \
      publish(data); \
      return ptr(ret); \
   } \
   template<class OtherType> \
   static ptr clone(typename OtherType::const_ptr other) { \
      return ObjType::clone<OtherType>(std::const_pointer_cast<OtherType>(other)); \
   } \
   static void destroy(const std::string &name) { shm<ObjType::Data>::destroy(name); } \
   void refresh() const override { Base::refresh(); refreshImpl(); } \
   void refreshImpl() const; \
   ObjType(Object::InitializedFlags) : Base(ObjType::Data::create()) { refreshImpl(); }  \
   virtual bool isEmpty() const override; \
   bool check() const override { refresh(); if (isEmpty()) {}; if (!Base::check()) return false; return checkImpl(); } \
   struct Data; \
   const Data *d() const { return static_cast<Data *>(Object::m_data); } \
   Data *d() { return static_cast<Data *>(Object::m_data); } \
   /* ARCHIVE_REGISTRATION(override) */ \
   ARCHIVE_REGISTRATION_INLINE \
   protected: \
   bool checkImpl() const; \
   ObjType(Data *data); \
   ObjType(); \
   private: \
   ARCHIVE_ACCESS_SPLIT \
   template<class Archive> \
      void load(Archive &ar) { \
         std::string name; \
         ar & V_NAME(ar, "name", name); \
         int type = Object::UNKNOWN; \
         ar & V_NAME(ar, "type", type); \
         if (!ar.currentObject()) \
            ar.setCurrentObject(Object::m_data); \
         d()->template serialize<Archive>(ar); \
         assert(type == Object::getType()); \
      } \
   template<class Archive> \
      void save(Archive &ar) const { \
         int type = Object::getType(); \
         std::string name = d()->name; \
         ar & V_NAME(ar, "name", name); \
         ar & V_NAME(ar, "type", type); \
         const_cast<Data *>(d())->template serialize<Archive>(ar); \
      } \
   friend std::shared_ptr<const Object> Shm::getObjectFromHandle(const shm_handle_t &) const; \
   friend shm_handle_t Shm::getHandleFromObject(std::shared_ptr<const Object>) const; \
   friend Object *Object::create(Object::Data *); \
   friend class ObjectTypeRegistry

#define V_DATA_BEGIN(ObjType) \
   public: \
   struct V_COREEXPORT Data: public Base::Data { \
      static Data *createNamed(Object::Type id, const std::string &name); \
      Data(Object::Type id, const std::string &name, const Meta &meta); \
      Data(const Data &other, const std::string &name) \

#define V_DATA_END(ObjType) \
      private: \
      friend class ObjType; \
      ARCHIVE_ACCESS \
      template<class Archive> \
      void serialize(Archive &ar); \
      void initData(); \
   }

#define V_OBJECT_DECLARE(ObjType) \
    namespace boost { template<> \
    struct is_virtual_base_of<ObjType::Base,ObjType>: public mpl::true_ {}; \
    }

#define V_SERIALIZERS4(Type1, Type2, prefix1, prefix2)

#define V_SERIALIZERS2(ObjType, prefix) \
    /* ARCHIVE_REGISTRATION_IMPL(ObjType, prefix) */

#define V_SERIALIZERS(ObjType) V_SERIALIZERS2(ObjType,)

#ifdef VISTLE_STATIC
#define REGISTER_TYPE(ObjType, id) \
{ \
   ObjectTypeRegistry::registerType<ObjType >(id); \
  ); \
}

#define V_INIT_STATIC
#else
#define V_INIT_STATIC static
#endif

#define V_OBJECT_TYPE3INT(ObjType, suffix, id) \
      class RegisterObjectType_##suffix { \
         public: \
                 RegisterObjectType_##suffix() { \
                    ObjectTypeRegistry::registerType<ObjType >(id); \
                 } \
      }; \
      V_INIT_STATIC RegisterObjectType_##suffix registerObjectType_##suffix; \

//! register a new Object type (complex form, specify suffix for symbol names)
#define V_OBJECT_TYPE3(ObjType, suffix, id) \
      V_OBJECT_TYPE3INT(ObjType, suffix, id)

//! register a new Object type (complex form, specify suffix for symbol names)
#define V_OBJECT_TYPE4(Type1, Type2, suffix, id) \
      namespace suffix { \
      typedef Type1,Type2 ObjType; \
      V_OBJECT_TYPE3INT(ObjType, suffix, id) \
   }

#define V_OBJECT_TYPE_T(ObjType, id) \
   V_SERIALIZERS2(ObjType, template<>) \
   V_OBJECT_TYPE3(ObjType, ObjType, id)

//! register a new Object type (simple form for non-templates, symbol suffix determined automatically)
#define V_OBJECT_TYPE(ObjType, id) \
   V_OBJECT_TYPE3(ObjType, ObjType, id) \
   Object::Type ObjType::type() { \
      return id; \
   } \
   V_SERIALIZERS(ObjType)

#define V_OBJECT_CREATE_NAMED(ObjType) \
   ObjType::Data::Data(Object::Type id, const std::string &name, const Meta &meta) \
   : ObjType::Base::Data(id, name, meta) { initData(); } \
   ObjType::Data *ObjType::Data::createNamed(Object::Type id, const std::string &name) { \
      Data *t = shm<Data>::construct(name)(id, name, Meta()); \
      publish(t); \
      return t; \
   }

#define V_OBJECT_CTOR(ObjType) \
   V_OBJECT_CREATE_NAMED(ObjType) \
   ObjType::ObjType(ObjType::Data *data): ObjType::Base(data) { refreshImpl(); } \
   ObjType::ObjType(): ObjType::Base() { refreshImpl(); }

void V_COREEXPORT registerTypes();

V_ENUM_OUTPUT_OP(Type, Object)

} // namespace vistle
#endif

#ifdef VISTLE_IMPL
#include "object_impl.h"
#endif
