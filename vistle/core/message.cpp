#include "message.h"
#include "shm.h"
#include "parameter.h"
#include "port.h"

#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/nil_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>

#include <algorithm>

namespace vistle {
namespace message {

template<typename T>
static T min(T a, T b) { return a<b ? a : b; }

#define COPY_STRING(dst, src) \
   { \
      const size_t size = min(src.size(), dst.size()-1); \
      src.copy(dst.data(), size); \
      dst[size] = '\0'; \
      vassert(src.size() < dst.size()); \
   }

DefaultSender DefaultSender::s_instance;

DefaultSender::DefaultSender()
: m_id(Id::Invalid)
, m_rank(-1)
{
   Router::the();
}

int DefaultSender::id() {

   return s_instance.m_id;
}

int DefaultSender::rank() {

   return s_instance.m_rank;
}

void DefaultSender::init(int id, int rank) {

   s_instance.m_id = id;
   s_instance.m_rank = rank;
}

const DefaultSender &DefaultSender::instance() {

   return s_instance;
}

static boost::uuids::random_generator s_uuidGenerator = boost::uuids::random_generator();

Message::Message(const Type t, const unsigned int s)
: m_broadcast(false)
, m_uuid(t == Message::ANY ? boost::uuids::nil_generator()() : s_uuidGenerator())
, m_size(s)
, m_type(t)
, m_senderId(DefaultSender::id())
, m_rank(DefaultSender::rank())
, m_destId(Id::NextHop)
, m_destRank(-1)
{

   vassert(m_size <= MESSAGE_SIZE);
   vassert(m_type > INVALID);
   vassert(m_type < NumMessageTypes);
}

unsigned long Message::typeFlags() const {

   vassert(type() > ANY && type() > INVALID && type() < NumMessageTypes);
   if (type() <= ANY || type() <= INVALID) {
      return 0;
   }
   if (type() >= NumMessageTypes) {
      return 0;
   }

   return Router::rt[type()];
}

const uuid_t &Message::uuid() const {

   return m_uuid;
}

void Message::setUuid(const uuid_t &uuid) {

   m_uuid = uuid;
}

int Message::senderId() const {

   return m_senderId;
}

void Message::setSenderId(int id) {

   m_senderId = id;
}

int Message::destId() const {

   return m_destId;
}

void Message::setDestId(int id) {

   m_destId = id;
}

int Message::destRank() const {
   return m_destRank;
}

void Message::setDestRank(int r) {
   m_destRank = r;
}

int Message::rank() const {

   return m_rank;
}

void Message::setRank(int rank) {
   
   m_rank = rank;
}

Message::Type Message::type() const {

   return m_type;
}

size_t Message::size() const {

   return m_size;
}

bool Message::broadcast() const {

    return m_broadcast;
}

void Message::setBroadcast(bool enable) {
    m_broadcast = enable;
}

Identify::Identify(Identity id, const std::string &name)
: Message(Message::IDENTIFY, sizeof(Identify))
, m_identity(id)
, m_id(Id::Invalid)
, m_rank(-1)
{
   COPY_STRING(m_name, name);
}

Identify::Identify(Identity id, int rank)
: Message(Message::IDENTIFY, sizeof(Identify))
, m_identity(id)
, m_id(Id::Invalid)
, m_rank(rank)
{
   vassert(id == Identify::LOCALBULKDATA || id == Identify::REMOTEBULKDATA);

   memset(m_name.data(), 0, m_name.size());
}

Identify::Identity Identify::identity() const {

   return m_identity;
}

const char *Identify::name() const {

   return m_name.data();
}

int Identify::id() const {

   return m_id;
}

int Identify::rank() const {

   return m_rank;
}

AddHub::AddHub(int id, const std::string &name)
: Message(Message::ADDHUB, sizeof(AddHub))
, m_id(id)
, m_port(0)
, m_addrType(AddHub::Unspecified)
{
   COPY_STRING(m_name, name);
   memset(m_address.data(), 0, m_address.size());
}

int AddHub::id() const {
   return m_id;
}

const char *AddHub::name() const {
   return m_name.data();
}

unsigned short AddHub::port() const {

   return m_port;
}

AddHub::AddressType AddHub::addressType() const {
   return m_addrType;
}

bool AddHub::hasAddress() const {
   return m_addrType == IPv6 || m_addrType == IPv4;
}

std::string AddHub::host() const {
   return m_address.data();
}

boost::asio::ip::address AddHub::address() const {
   vassert(hasAddress());
   if (addressType() == IPv6)
      return addressV6();
   else
      return addressV4();
}

boost::asio::ip::address_v6 AddHub::addressV6() const {
   vassert(m_addrType == IPv6);
   return boost::asio::ip::address_v6::from_string(m_address.data());
}

boost::asio::ip::address_v4 AddHub::addressV4() const {
   vassert(m_addrType == IPv4);
   return boost::asio::ip::address_v4::from_string(m_address.data());
}

void AddHub::setPort(unsigned short port) {
   m_port = port;
}

void AddHub::setAddress(boost::asio::ip::address addr) {
   vassert(addr.is_v4() || addr.is_v6());

   if (addr.is_v4())
      setAddress(addr.to_v4());
   else if (addr.is_v6())
      setAddress(addr.to_v6());
}

void AddHub::setAddress(boost::asio::ip::address_v6 addr) {
   const std::string addrString = addr.to_string();
   COPY_STRING(m_address, addrString);
   m_addrType = IPv6;
}

void AddHub::setAddress(boost::asio::ip::address_v4 addr) {
   const std::string addrString = addr.to_string();
   COPY_STRING(m_address, addrString);
   m_addrType = IPv4;
}


Ping::Ping(const char c)
   : Message(Message::PING, sizeof(Ping)), character(c) {

}

char Ping::getCharacter() const {

   return character;
}

Pong::Pong(const Ping &ping)
   : Message(Message::PONG, sizeof(Pong)), character(ping.getCharacter()), module(ping.senderId()) {

   setUuid(ping.uuid());
}

char Pong::getCharacter() const {

   return character;
}

int Pong::getDestination() const {

   return module;
}

Spawn::Spawn(int hub,
             const std::string & n, int mpiSize, int baseRank, int rankSkip)
   : Message(Message::SPAWN, sizeof(Spawn))
   , m_hub(hub)
   , m_spawnId(Id::Invalid)
   , mpiSize(mpiSize)
   , baseRank(baseRank)
   , rankSkip(rankSkip)
{

   COPY_STRING(name, n);
}

int Spawn::hubId() const {

   return m_hub;
}

int Spawn::spawnId() const {

   return m_spawnId;
}

void Spawn::setSpawnId(int id) {

   m_spawnId = id;
}

const char * Spawn::getName() const {

   return name.data();
}

int Spawn::getMpiSize() const {

   return mpiSize;
}

int Spawn::getBaseRank() const {

   return baseRank;
}

int Spawn::getRankSkip() const {

   return rankSkip;
}

SpawnPrepared::SpawnPrepared(const Spawn &spawn)
   : Message(Message::SPAWNPREPARED, sizeof(SpawnPrepared))
   , m_hub(spawn.hubId())
   , m_spawnId(spawn.spawnId())
{

   setUuid(spawn.uuid());
   COPY_STRING(name, std::string(spawn.getName()));
}

int SpawnPrepared::hubId() const {

   return m_hub;
}

int SpawnPrepared::spawnId() const {

   return m_spawnId;
}

const char * SpawnPrepared::getName() const {

   return name.data();
}

Started::Started(const std::string &n)
: Message(Message::STARTED, sizeof(Started))
{

   COPY_STRING(name, n);
}

const char * Started::getName() const {

   return name.data();
}

Kill::Kill(const int m)
   : Message(Message::KILL, sizeof(Kill)), module(m) {
}

int Kill::getModule() const {

   return module;
}

Quit::Quit()
   : Message(Message::QUIT, sizeof(Quit)) {
}

ModuleExit::ModuleExit()
: Message(Message::MODULEEXIT, sizeof(ModuleExit))
, forwarded(false)
{

}

void ModuleExit::setForwarded() {

   forwarded = true;
}

bool ModuleExit::isForwarded() const {

   return forwarded;
}

Execute::Execute(Execute::What what, const int module, const int count)
   : Message(Message::EXECUTE, sizeof(Execute))
   , m_allRanks(false)
   , module(module)
   , executionCount(count)
   , m_what(what)
{
}

void Execute::setModule(int m) {

   module = m;
}

int Execute::getModule() const {

   return module;
}

void Execute::setExecutionCount(int count) {

   executionCount = count;
}

int Execute::getExecutionCount() const {

   return executionCount;
}

bool Execute::allRanks() const
{
   return m_allRanks;
}

void Execute::setAllRanks(bool allRanks)
{
   m_allRanks = allRanks;
}

Execute::What Execute::what() const
{
   return m_what;
}

void Execute::setWhat(Execute::What what)
{
   m_what = what;
}


Busy::Busy()
: Message(Message::BUSY, sizeof(Busy)) {
}

Idle::Idle()
: Message(Message::IDLE, sizeof(Idle)) {
}

AddPort::AddPort(const Port *port)
: Message(Message::ADDPORT, sizeof(AddPort))
, m_porttype(port->getType())
, m_flags(port->flags())
{
   COPY_STRING(m_name, port->getName());
}

Port *AddPort::getPort() const {

   return new Port(senderId(), m_name.data(), static_cast<Port::Type>(m_porttype), m_flags);
}

AddObject::AddObject(const std::string &sender, vistle::Object::const_ptr obj,
      const std::string & dest)
: Message(Message::ADDOBJECT, sizeof(AddObject))
, m_name(obj->getName())
, m_meta(obj->meta())
, m_objectType(obj->getType())
, handle(obj->getHandle())
, m_handleValid(true)
{
   // we keep the handle as a reference to obj
   obj->ref();

   COPY_STRING(senderPort, sender);
   COPY_STRING(destPort, dest);
   COPY_STRING(m_shmname, Shm::the().name());
}

AddObject::AddObject(const AddObject &o)
: Message(o)
, senderPort(o.senderPort)
, destPort(o.destPort)
, m_name(o.m_name)
, m_shmname(o.m_shmname)
, m_meta(o.m_meta)
, m_objectType(o.m_objectType)
, handle(o.handle)
, m_handleValid(false)
{
    setUuid(o.uuid());

    if (Shm::isAttached() && Shm::the().name() == std::string(m_shmname.data())) {
        vistle::Object::const_ptr obj = Shm::the().getObjectFromHandle(handle);
        obj->ref();
        m_handleValid = true;
    }
}

AddObject::~AddObject() {
}

const char * AddObject::getSenderPort() const {

   return senderPort.data();
}

void AddObject::setDestPort(const std::string &dest) {
   COPY_STRING(destPort, dest);
}

const char * AddObject::getDestPort() const {

   return destPort.data();
}

const char *AddObject::objectName() const {

   return m_name;
}

const shm_handle_t & AddObject::getHandle() const {

   vassert(m_handleValid);
   return handle;
}

const Meta &AddObject::meta() const {
   return m_meta;
}

Object::Type AddObject::objectType() const {
   return static_cast<Object::Type>(m_objectType);
}

Object::const_ptr AddObject::takeObject() const {

   if (Shm::isAttached() && Shm::the().name() == std::string(m_shmname.data())) {
      vassert(m_handleValid);
      vistle::Object::const_ptr obj = Shm::the().getObjectFromHandle(handle);
      if (obj) {
         // ref count has been increased during AddObject construction
         obj->unref();
         m_handleValid = false;
      } else {
          std::cerr << "did not find " << m_name << " by handle" << std::endl;
      }
      return obj;
   }
   auto obj = Shm::the().getObjectFromName(m_name);
   if (!obj) {
      std::cerr << "did not find " << m_name << " by name" << std::endl;
   }
   return obj;
}


AddObjectCompleted::AddObjectCompleted(const AddObject &msg)
: Message(Message::ADDOBJECTCOMPLETED, sizeof(AddObjectCompleted))
, m_orgSenderId(msg.senderId())
{
   COPY_STRING(m_orgSenderPort, std::string(msg.getSenderPort()));
}

int AddObjectCompleted::originalSenderId() const {
   return m_orgSenderId;
}

const char *AddObjectCompleted::originalSenderPort() const {
   return m_orgSenderPort.data();
}

ObjectReceived::ObjectReceived(const std::string &sender, vistle::Object::const_ptr obj,
      const std::string &p)
: Message(Message::OBJECTRECEIVED, sizeof(ObjectReceived))
, m_name(obj->getName())
, m_meta(obj->meta())
, m_objectType(obj->getType())
{

   m_broadcast = true;

   COPY_STRING(senderPort, sender);
   COPY_STRING(portName, p);
}

const Meta &ObjectReceived::meta() const {

   return m_meta;
}

const char *ObjectReceived::getSenderPort() const {

   return senderPort.data();
}

const char *ObjectReceived::getDestPort() const {

   return portName.data();
}

const char *ObjectReceived::objectName() const {

   return m_name;
}

Object::Type ObjectReceived::objectType() const {

   return static_cast<Object::Type>(m_objectType);
}

Connect::Connect(const int moduleIDA, const std::string & portA,
                 const int moduleIDB, const std::string & portB)
   : Message(Message::CONNECT, sizeof(Connect)),
     moduleA(moduleIDA), moduleB(moduleIDB) {

        COPY_STRING(portAName, portA);
        COPY_STRING(portBName, portB);
}

const char * Connect::getPortAName() const {

   return portAName.data();
}

const char * Connect::getPortBName() const {

   return portBName.data();
}

int Connect::getModuleA() const {

   return moduleA;
}

int Connect::getModuleB() const {

   return moduleB;
}

void Connect::reverse() {

   std::swap(moduleA, moduleB);
   std::swap(portAName, portBName);
}

Disconnect::Disconnect(const int moduleIDA, const std::string & portA,
                 const int moduleIDB, const std::string & portB)
   : Message(Message::DISCONNECT, sizeof(Disconnect)),
     moduleA(moduleIDA), moduleB(moduleIDB) {

        COPY_STRING(portAName, portA);
        COPY_STRING(portBName, portB);
}

const char * Disconnect::getPortAName() const {

   return portAName.data();
}

const char * Disconnect::getPortBName() const {

   return portBName.data();
}

int Disconnect::getModuleA() const {

   return moduleA;
}

int Disconnect::getModuleB() const {

   return moduleB;
}

void Disconnect::reverse() {

   std::swap(moduleA, moduleB);
   std::swap(portAName, portBName);
}

AddParameter::AddParameter(const Parameter &param, const std::string &modname)
: Message(Message::ADDPARAMETER, sizeof(AddParameter))
, paramtype(param.type())
, presentation(param.presentation())
{
   vassert(paramtype > Parameter::Unknown);
   vassert(paramtype < Parameter::Invalid);

   vassert(presentation >= Parameter::Generic);
   vassert(presentation <= Parameter::InvalidPresentation);

   COPY_STRING(name, param.getName());
   COPY_STRING(m_group, param.group());
   COPY_STRING(module, modname);
   COPY_STRING(m_description, param.description());
}

const char *AddParameter::getName() const {

   return name.data();
}

const char *AddParameter::moduleName() const {

   return module.data();
}

int AddParameter::getParameterType() const {

   return paramtype;
}

int AddParameter::getPresentation() const {

   return presentation;
}

const char *AddParameter::description() const {

   return m_description.data();
}

const char *AddParameter::group() const {

   return m_group.data();
}

boost::shared_ptr<Parameter> AddParameter::getParameter() const {

   boost::shared_ptr<Parameter> p;
   switch (getParameterType()) {
      case Parameter::Integer:
         p.reset(new IntParameter(senderId(), getName()));
         break;
      case Parameter::Float:
         p.reset(new FloatParameter(senderId(), getName()));
         break;
      case Parameter::Vector:
         p.reset(new VectorParameter(senderId(), getName()));
         break;
      case Parameter::IntVector:
         p.reset(new IntVectorParameter(senderId(), getName()));
         break;
      case Parameter::String:
         p.reset(new StringParameter(senderId(), getName()));
         break;
      case Parameter::Invalid:
      case Parameter::Unknown:
         break;
   }

   if (p) {

      p->setDescription(description());
      p->setGroup(group());
      p->setPresentation(Parameter::Presentation(getPresentation()));
   } else {

      std::cerr << "AddParameter::getParameter (" <<
         moduleName() << ":" << getName()
         << ": type " << getParameterType() << " not handled" << std::endl;
      vassert("parameter type not supported" == 0);
   }

   return p;
}

SetParameter::SetParameter(const int module,
      const std::string &n, const boost::shared_ptr<Parameter> param, Parameter::RangeType rt)
: Message(Message::SETPARAMETER, sizeof(SetParameter))
, module(module)
, paramtype(param->type())
, initialize(false)
, reply(false)
, rangetype(rt)
{

   COPY_STRING(name, n);
   if (const auto pint = boost::dynamic_pointer_cast<const IntParameter>(param)) {
      v_int = pint->getValue(rt);
   } else if (const auto pfloat = boost::dynamic_pointer_cast<const FloatParameter>(param)) {
      v_scalar = pfloat->getValue(rt);
   } else if (const auto pvec = boost::dynamic_pointer_cast<const VectorParameter>(param)) {
      ParamVector v = pvec->getValue(rt);
      dim = v.dim;
      for (int i=0; i<MaxDimension; ++i)
         v_vector[i] = v[i];
   } else if (const auto ipvec = boost::dynamic_pointer_cast<const IntVectorParameter>(param)) {
      IntParamVector v = ipvec->getValue(rt);
      dim = v.dim;
      for (int i=0; i<MaxDimension; ++i)
         v_ivector[i] = v[i];
   } else if (const auto pstring = boost::dynamic_pointer_cast<const StringParameter>(param)) {
      COPY_STRING(v_string, pstring->getValue(rt));
   } else {
      std::cerr << "SetParameter: type " << param->type() << " not handled" << std::endl;
      vassert("invalid parameter type" == 0);
   }
}

SetParameter::SetParameter(const int module,
      const std::string &n, const Integer v)
: Message(Message::SETPARAMETER, sizeof(SetParameter))
, module(module)
, paramtype(Parameter::Integer)
, initialize(false)
, reply(false)
, rangetype(Parameter::Value)
{

   COPY_STRING(name, n);
   v_int = v;
}

SetParameter::SetParameter(const int module,
      const std::string &n, const Float v)
: Message(Message::SETPARAMETER, sizeof(SetParameter))
, module(module)
, paramtype(Parameter::Float)
, initialize(false)
, reply(false)
, rangetype(Parameter::Value)
{

   COPY_STRING(name, n);
   v_scalar = v;
}

SetParameter::SetParameter(const int module,
      const std::string &n, const ParamVector &v)
: Message(Message::SETPARAMETER, sizeof(SetParameter))
, module(module)
, paramtype(Parameter::Vector)
, initialize(false)
, reply(false)
, rangetype(Parameter::Value)
{

   COPY_STRING(name, n);
   dim = v.dim;
   for (int i=0; i<MaxDimension; ++i)
      v_vector[i] = v[i];
}

SetParameter::SetParameter(const int module,
      const std::string &n, const IntParamVector &v)
: Message(Message::SETPARAMETER, sizeof(SetParameter))
, module(module)
, paramtype(Parameter::IntVector)
, initialize(false)
, reply(false)
, rangetype(Parameter::Value)
{

   COPY_STRING(name, n);
   dim = v.dim;
   for (int i=0; i<MaxDimension; ++i)
      v_ivector[i] = v[i];
}

SetParameter::SetParameter(const int module,
      const std::string &n, const std::string &v)
: Message(Message::SETPARAMETER, sizeof(SetParameter))
, module(module)
, paramtype(Parameter::String)
, initialize(false)
, reply(false)
, rangetype(Parameter::Value)
{

   COPY_STRING(name, n);
   COPY_STRING(v_string, v);
}

void SetParameter::setInit() {

   initialize = true;
}

bool SetParameter::isInitialization() const {

   return initialize;
}

void SetParameter::setReply() {

   reply = true;
}

bool SetParameter::isReply() const {

   return reply;
}

void SetParameter::setRangeType(int rt) {

   vassert(rt >= Parameter::Minimum);
   vassert(rt <= Parameter::Maximum);
   rangetype = rt;
}

int SetParameter::rangeType() const {

   return rangetype;
}

const char *SetParameter::getName() const {

   return name.data();
}

int SetParameter::getModule() const {

   return module;
}

int SetParameter::getParameterType() const {

   return paramtype;
}

Integer SetParameter::getInteger() const {

   vassert(paramtype == Parameter::Integer);
   return v_int;
}

Float SetParameter::getFloat() const {

   vassert(paramtype == Parameter::Float);
   return v_scalar;
}

ParamVector SetParameter::getVector() const {

   vassert(paramtype == Parameter::Vector);
   return ParamVector(dim, &v_vector[0]);
}

IntParamVector SetParameter::getIntVector() const {

   vassert(paramtype == Parameter::IntVector);
   return IntParamVector(dim, &v_ivector[0]);
}

std::string SetParameter::getString() const {

   vassert(paramtype == Parameter::String);
   return v_string.data();
}

bool SetParameter::apply(boost::shared_ptr<vistle::Parameter> param) const {

   if (paramtype != param->type()) {
      std::cerr << "SetParameter::apply(): type mismatch for " << param->module() << ":" << param->getName() << std::endl;
      return false;
   }

   const int rt = rangeType();
   if (auto pint = boost::dynamic_pointer_cast<IntParameter>(param)) {
      if (rt == Parameter::Value) pint->setValue(v_int, initialize);
      if (rt == Parameter::Minimum) pint->setMinimum(v_int);
      if (rt == Parameter::Maximum) pint->setMaximum(v_int);
   } else if (auto pfloat = boost::dynamic_pointer_cast<FloatParameter>(param)) {
      if (rt == Parameter::Value) pfloat->setValue(v_scalar, initialize);
      if (rt == Parameter::Minimum) pfloat->setMinimum(v_scalar);
      if (rt == Parameter::Maximum) pfloat->setMaximum(v_scalar);
   } else if (auto pvec = boost::dynamic_pointer_cast<VectorParameter>(param)) {
      if (rt == Parameter::Value) pvec->setValue(ParamVector(dim, &v_vector[0]), initialize);
      if (rt == Parameter::Minimum) pvec->setMinimum(ParamVector(dim, &v_vector[0]));
      if (rt == Parameter::Maximum) pvec->setMaximum(ParamVector(dim, &v_vector[0]));
   } else if (auto pivec = boost::dynamic_pointer_cast<IntVectorParameter>(param)) {
      if (rt == Parameter::Value) pivec->setValue(IntParamVector(dim, &v_ivector[0]), initialize);
      if (rt == Parameter::Minimum) pivec->setMinimum(IntParamVector(dim, &v_ivector[0]));
      if (rt == Parameter::Maximum) pivec->setMaximum(IntParamVector(dim, &v_ivector[0]));
   } else if (auto pstring = boost::dynamic_pointer_cast<StringParameter>(param)) {
      if (rt == Parameter::Value) pstring->setValue(v_string.data(), initialize);
   } else {
      std::cerr << "SetParameter::apply(): type " << param->type() << " not handled" << std::endl;
      vassert("invalid parameter type" == 0);
   }
   
   return true;
}

SetParameterChoices::SetParameterChoices(const int id, const std::string &n,
      const std::vector<std::string> &ch)
: Message(Message::SETPARAMETERCHOICES, sizeof(SetParameterChoices))
, module(id)
, numChoices(ch.size())
{
   COPY_STRING(name, n);
   if (numChoices > param_num_choices) {
      std::cerr << "SetParameterChoices::ctor: maximum number of choices (" << param_num_choices << ") exceeded (" << numChoices << ") - truncating" << std::endl;
      numChoices = param_num_choices;
   }
   for (int i=0; i<numChoices; ++i) {
      COPY_STRING(choices[i], ch[i]);
   }
}

int SetParameterChoices::getModule() const
{
   return module;
}

const char *SetParameterChoices::getName() const
{
   return name.data();
}

bool SetParameterChoices::apply(boost::shared_ptr<vistle::Parameter> param) const {

   if (param->type() != Parameter::Integer
         && param->type() != Parameter::String) {
      std::cerr << "SetParameterChoices::apply(): parameter type not compatible with choice" << std::endl;
      return false;
   }

   if (param->presentation() != Parameter::Choice) {
      std::cerr << "SetParameterChoices::apply(): parameter presentation is not 'Choice'" << std::endl;
      return false;
   }

   std::vector<std::string> ch;
   for (int i=0; i<numChoices && i<param_num_choices; ++i) {
      size_t len = strnlen(choices[i].data(), choices[i].size());
      ch.push_back(std::string(choices[i].data(), len));
   }

   param->setChoices(ch);
   return true;
}

Barrier::Barrier()
: Message(Message::BARRIER, sizeof(Barrier))
{
}

BarrierReached::BarrierReached(const vistle::message::uuid_t &uuid)
: Message(Message::BARRIERREACHED, sizeof(BarrierReached))
{
   setUuid(uuid);
}

SetId::SetId(const int id)
: Message(Message::SETID, sizeof(SetId))
, m_id(id)
{
}

int SetId::getId() const {

   return m_id;
}

ReplayFinished::ReplayFinished()
   : Message(Message::REPLAYFINISHED, sizeof(ReplayFinished))
{
}

SendText::SendText(const std::string &text, const Message &inResponseTo)
: Message(Message::SENDTEXT, sizeof(SendText))
, m_textType(TextType::Error)
, m_referenceUuid(inResponseTo.uuid())
, m_referenceType(inResponseTo.type())
, m_truncated(false)
{
   if (text.size() >= sizeof(m_text)) {
      m_truncated = true;
   }
   COPY_STRING(m_text, text.substr(0, sizeof(m_text)-1));
}

SendText::SendText(SendText::TextType type, const std::string &text)
: Message(Message::SENDTEXT, sizeof(SendText))
, m_textType(type)
, m_referenceType(Message::INVALID)
, m_truncated(false)
{
   if (text.size() >= sizeof(m_text)) {
      m_truncated = true;
   }
   COPY_STRING(m_text, text.substr(0, sizeof(m_text)-1));
}

SendText::TextType SendText::textType() const {

   return m_textType;
}

Message::Type SendText::referenceType() const {

   return m_referenceType;
}

uuid_t SendText::referenceUuid() const {

   return m_referenceUuid;
}

const char *SendText::text() const {

   return m_text.data();
}

bool SendText::truncated() const {

   return m_truncated;
}

ObjectReceivePolicy::ObjectReceivePolicy(ObjectReceivePolicy::Policy pol)
: Message(Message::OBJECTRECEIVEPOLICY, sizeof(ObjectReceivePolicy))
, m_policy(pol)
{
}

ObjectReceivePolicy::Policy ObjectReceivePolicy::policy() const {

   return m_policy;
}

SchedulingPolicy::SchedulingPolicy(SchedulingPolicy::Schedule pol)
: Message(Message::SCHEDULINGPOLICY, sizeof(SchedulingPolicy))
, m_policy(pol)
{
}

SchedulingPolicy::Schedule SchedulingPolicy::policy() const {

   return m_policy;
}

ReducePolicy::ReducePolicy(ReducePolicy::Reduce red)
: Message(Message::REDUCEPOLICY, sizeof(ReducePolicy))
, m_reduce(red)
{
}

ReducePolicy::Reduce ReducePolicy::policy() const {

   return m_reduce;
}

ExecutionProgress::ExecutionProgress(Progress stage)
: Message(Message::EXECUTIONPROGRESS, sizeof(ExecutionProgress))
, m_stage(stage)
{
}

ExecutionProgress::Progress ExecutionProgress::stage() const {

   return m_stage;
}

void ExecutionProgress::setStage(ExecutionProgress::Progress stage) {

   m_stage = stage;
}

Trace::Trace(int module, Message::Type messageType, bool onoff)
: Message(Message::TRACE, sizeof(Trace))
, m_module(module)
, m_messageType(messageType)
, m_on(onoff)
{
}

int Trace::module() const {

   return m_module;
}

Message::Type Trace::messageType() const {

   return m_messageType;
}

bool Trace::on() const {

   return m_on;
}

ModuleAvailable::ModuleAvailable(int hub, const std::string &name, const std::string &path)
: Message(Message::MODULEAVAILABLE, sizeof(ModuleAvailable))
, m_hub(hub)
{

   COPY_STRING(m_name, name);
   COPY_STRING(m_path, path);
}

int ModuleAvailable::hub() const {

   return m_hub;
}

const char *ModuleAvailable::name() const {

    return m_name.data();
}

const char *ModuleAvailable::path() const {

   return m_path.data();
}

LockUi::LockUi(bool locked)
: Message(Message::LOCKUI, sizeof(LockUi))
, m_locked(locked)
{
}

bool LockUi::locked() const {

   return m_locked;
}

RequestTunnel::RequestTunnel(unsigned short srcPort, const std::string &destHost, unsigned short destPort)
: Message(Message::REQUESTTUNNEL, sizeof(RequestTunnel))
, m_srcPort(srcPort)
, m_destType(RequestTunnel::Hostname)
, m_destPort(destPort)
, m_remove(false)
{
   COPY_STRING(m_destAddr, destHost);
}

RequestTunnel::RequestTunnel(unsigned short srcPort, const boost::asio::ip::address_v6 destAddr, unsigned short destPort)
: Message(Message::REQUESTTUNNEL, sizeof(RequestTunnel))
, m_srcPort(srcPort)
, m_destType(RequestTunnel::IPv6)
, m_destPort(destPort)
, m_remove(false)
{
   const std::string addr = destAddr.to_string();
   COPY_STRING(m_destAddr, addr);
}

RequestTunnel::RequestTunnel(unsigned short srcPort, const boost::asio::ip::address_v4 destAddr, unsigned short destPort)
: Message(Message::REQUESTTUNNEL, sizeof(RequestTunnel))
, m_srcPort(srcPort)
, m_destType(RequestTunnel::IPv4)
, m_destPort(destPort)
, m_remove(false)
{
   const std::string addr = destAddr.to_string();
   COPY_STRING(m_destAddr, addr);
}

RequestTunnel::RequestTunnel(unsigned short srcPort, unsigned short destPort)
: Message(Message::REQUESTTUNNEL, sizeof(RequestTunnel))
, m_srcPort(srcPort)
, m_destType(RequestTunnel::Unspecified)
, m_destPort(destPort)
, m_remove(false)
{
   const std::string empty;
   COPY_STRING(m_destAddr, empty);
}

RequestTunnel::RequestTunnel(unsigned short srcPort)
: Message(Message::REQUESTTUNNEL, sizeof(RequestTunnel))
, m_srcPort(srcPort)
, m_destType(RequestTunnel::Unspecified)
, m_destPort(0)
, m_remove(true)
{
   const std::string empty;
   COPY_STRING(m_destAddr, empty);
}

unsigned short RequestTunnel::srcPort() const {
   return m_srcPort;
}

unsigned short RequestTunnel::destPort() const {
   return m_destPort;
}

RequestTunnel::AddressType RequestTunnel::destType() const {
   return m_destType;
}

bool RequestTunnel::destIsAddress() const {
   return m_destType == IPv6 || m_destType == IPv4;
}

void RequestTunnel::setDestAddr(boost::asio::ip::address_v4 addr) {
   const std::string addrString = addr.to_string();
   COPY_STRING(m_destAddr, addrString);
   m_destType = IPv4;
}

void RequestTunnel::setDestAddr(boost::asio::ip::address_v6 addr) {
   const std::string addrString = addr.to_string();
   COPY_STRING(m_destAddr, addrString);
   m_destType = IPv6;
}

boost::asio::ip::address RequestTunnel::destAddr() const {
   vassert(destIsAddress());
   if (destType() == IPv6)
      return destAddrV6();
   else
      return destAddrV4();
}

boost::asio::ip::address_v6 RequestTunnel::destAddrV6() const {
   vassert(m_destType == IPv6);
   return boost::asio::ip::address_v6::from_string(m_destAddr.data());
}

boost::asio::ip::address_v4 RequestTunnel::destAddrV4() const {
   vassert(m_destType == IPv4);
   return boost::asio::ip::address_v4::from_string(m_destAddr.data());
}

std::string RequestTunnel::destHost() const {
   return m_destAddr.data();
}

bool RequestTunnel::remove() const {
   return m_remove;
}

RequestObject::RequestObject(const AddObject &add, const std::string &objId, const std::string &referrer, bool array)
: Message(Message::REQUESTOBJECT, sizeof(RequestObject))
, m_objectId(objId)
, m_referrer(referrer.empty() ? add.objectName() : referrer)
, m_array(array)
{
   setUuid(add.uuid());
   setDestId(add.senderId());
   setDestRank(add.rank());
}

RequestObject::RequestObject(int destId, int destRank, const std::string &objId, const std::string &referrer, bool array)
: Message(Message::REQUESTOBJECT, sizeof(RequestObject))
, m_objectId(objId)
, m_referrer(referrer)
, m_array(array)
{
   setDestId(destId);
   setDestRank(destRank);
}

const char *RequestObject::objectId() const {
   return m_objectId;
}

const char *RequestObject::referrer() const {
   return m_referrer;
}

bool RequestObject::isArray() const {
   return m_array;
}


SendObject::SendObject(const RequestObject &request, Object::const_ptr obj, size_t payloadSize)
: Message(Message::SENDOBJECT, sizeof(SendObject))
, m_objectId(obj->getName())
, m_objectType(obj->getType())
, m_meta(obj->meta())
, m_payloadSize(payloadSize)
{
   setUuid(request.uuid());

   auto &meta = obj->meta();
   m_block = meta.block();
   m_numBlocks = meta.numBlocks();
   m_timestep = meta.timeStep();
   m_numTimesteps = meta.numTimesteps();
   m_animationstep = meta.animationStep();
   m_numAnimationsteps = meta.numAnimationSteps();
   m_iteration = meta.iteration();
   m_executionCount = meta.executionCounter();
   m_creator = meta.creator();
   m_realtime = meta.realTime();
}

const char *SendObject::objectId() const {
   return m_objectId;
}

size_t SendObject::payloadSize() const {
   return m_payloadSize;
}

const Meta &SendObject::meta() const {
   return m_meta;
}

Object::Type SendObject::objectType() const {
   return static_cast<Object::Type>(m_objectType);
}


Meta SendObject::objectMeta() const {

   Meta meta(m_block, m_timestep, m_animationstep, m_iteration, m_executionCount, m_creator);
   meta.setNumBlocks(m_numBlocks);
   meta.setNumTimesteps(m_numTimesteps);
   meta.setNumAnimationSteps(m_numAnimationsteps);
   meta.setRealTime(m_realtime);
   return meta;
}

std::ostream &operator<<(std::ostream &s, const Message &m) {

   using namespace vistle::message;

   s  << "uuid: " << boost::lexical_cast<std::string>(m.uuid())
      << ", type: " << m.type()
      << ", size: " << m.size()
      << ", sender: " << m.senderId()
      << ", dest: " << m.destId()
      << ", rank: " << m.rank();

   switch (m.type()) {
      case Message::EXECUTE: {
         auto mm = static_cast<const Execute &>(m);
         s << ", module: " << mm.getModule() << ", what: " << mm.what() << ", execcount: " << mm.getExecutionCount();
         break;
      }
      case Message::EXECUTIONPROGRESS: {
         auto mm = static_cast<const ExecutionProgress &>(m);
         s << ", stage: " << ExecutionProgress::toString(mm.stage());
         break;
      }
      case Message::ADDPARAMETER: {
         auto mm = static_cast<const AddParameter &>(m);
         s << ", name: " << mm.getName();
         break;
      }
      case Message::SETPARAMETER: {
         auto mm = static_cast<const SetParameter &>(m);
         s << ", dest: " << mm.getModule() << ", name: " << mm.getName();
         break;
      }
      case Message::SETPARAMETERCHOICES: {
         auto mm = static_cast<const SetParameterChoices &>(m);
         s << ", dest: " << mm.getModule() << ", name: " << mm.getName();
         break;
      }
      case Message::ADDPORT: {
         auto mm = static_cast<const AddPort &>(m);
         s << ", name: " << mm.getPort()->getName();
         break;
      }
      case Message::MODULEAVAILABLE: {
         auto mm = static_cast<const ModuleAvailable &>(m);
         s << ", name: " << mm.name() << ", hub: " << mm.hub();
         break;
      }
      case Message::SPAWN: {
         auto mm = static_cast<const Spawn &>(m);
         s << ", name: " << mm.getName() << ", id: " << mm.spawnId() << ", hub: " << mm.hubId();
         break;
      }
      case Message::ADDHUB: {
         auto mm = static_cast<const AddHub &>(m);
         s << ", name: " << mm.name() << ", id: " << mm.id();
         break;
      }
      case Message::ADDOBJECT: {
         auto mm = static_cast<const AddObject &>(m);
         s << ", obj: " << mm.objectName() << ", " << mm.getSenderPort() << " -> " << mm.getDestPort();
         break;
      }
      case Message::REQUESTOBJECT: {
         auto mm = static_cast<const RequestObject &>(m);
         s << ", obj: " << mm.objectId();
         break;
      }
      case Message::SENDOBJECT: {
         auto mm = static_cast<const SendObject &>(m);
         s << ", obj: " << mm.objectId() << ", payload: " << mm.payloadSize();
         break;
      }
      default:
         break;
   }

   return s;
}


unsigned Router::rt[Message::NumMessageTypes];

void Router::initRoutingTable() {

   typedef Message M;
   memset(&rt, '\0', sizeof(rt));

   rt[M::INVALID]               = 0;
   rt[M::IDENTIFY]              = Special;
   rt[M::SETID]                 = Special;
   rt[M::ADDHUB]                = Broadcast|Track|DestUi;
   rt[M::REMOVESLAVE]           = Broadcast|Track|DestUi;
   rt[M::REPLAYFINISHED]        = Special;
   rt[M::TRACE]                 = Broadcast|Track;
   rt[M::SPAWN]                 = Track|HandleOnMaster;
   rt[M::SPAWNPREPARED]         = DestLocalHub|HandleOnHub;
   rt[M::STARTED]               = Broadcast|Track|DestUi;
   rt[M::MODULEEXIT]            = Broadcast|Track|DestUi;
   rt[M::KILL]                  = DestModules|HandleOnDest;
   rt[M::QUIT]                  = Broadcast|HandleOnMaster|HandleOnHub|HandleOnNode;
   rt[M::EXECUTE]               = Special|HandleOnMaster;
   rt[M::MODULEAVAILABLE]       = Track|DestHub|DestUi|HandleOnHub;
   rt[M::ADDPORT]               = Broadcast|Track|DestUi|TriggerQueue;
   rt[M::ADDPARAMETER]          = Broadcast|Track|DestUi|TriggerQueue;
   rt[M::CONNECT]               = Track|Broadcast|QueueIfUnhandled|DestManager;
   rt[M::DISCONNECT]            = Track|Broadcast|QueueIfUnhandled|DestManager;
   rt[M::SETPARAMETER]          = Track|QueueIfUnhandled|DestManager;
   rt[M::SETPARAMETERCHOICES]   = Track|QueueIfUnhandled|DestManager;
   rt[M::PING]                  = DestModules|HandleOnDest;
   rt[M::PONG]                  = DestUi|HandleOnDest;
   rt[M::BUSY]                  = Special;
   rt[M::IDLE]                  = Special;
   rt[M::LOCKUI]                = DestUi;
   rt[M::SENDTEXT]              = DestUi|DestMasterHub;

   rt[M::OBJECTRECEIVEPOLICY]   = DestLocalManager|Track;
   rt[M::SCHEDULINGPOLICY]      = DestLocalManager|Track;
   rt[M::REDUCEPOLICY]          = DestLocalManager|Track;
   rt[M::EXECUTIONPROGRESS]     = DestLocalManager|HandleOnRank0;

   rt[M::ADDOBJECT]             = DestLocalManager|HandleOnNode;
   rt[M::ADDOBJECTCOMPLETED]    = Special;

   rt[M::BARRIER]               = HandleOnDest;
   rt[M::BARRIERREACHED]        = HandleOnDest;
   rt[M::OBJECTRECEIVED]        = HandleOnRank0;

   rt[M::REQUESTTUNNEL]         = HandleOnNode|HandleOnHub;

   rt[M::REQUESTOBJECT]         = Special;
   rt[M::SENDOBJECT]            = Special;

   for (int i=M::ANY+1; i<M::NumMessageTypes; ++i) {
      if (rt[i] == 0) {
         std::cerr << "message routing table not initialized for " << (Message::Type)i << std::endl;
      }
      vassert(rt[i] != 0);
   }
}

Router &Router::the() {

   static Router router;
   return router;
}

Router::Router() {

   m_identity = Identify::UNKNOWN;
   m_id = Id::Invalid;
   initRoutingTable();
}

void Router::init(Identify::Identity identity, int id) {

   the().m_identity = identity;
   the().m_id = id;
}

bool Router::toUi(const Message &msg, Identify::Identity senderType) {

   if (msg.destId() == Id::ForBroadcast)
      return false;
   if (msg.destId() >= Id::ModuleBase)
      return false;
   if (msg.destId() == Id::Broadcast)
      return true;
   if (msg.destId() == Id::UI)
      return true;
   const int t = msg.type();
   if (rt[t] & DestUi)
      return true;
   if (rt[t] & Broadcast)
      return true;

   return false;
}

bool Router::toMasterHub(const Message &msg, Identify::Identity senderType, int senderHub) {

   if (m_identity != Identify::SLAVEHUB)
      return false;
   if (senderType == Identify::HUB)
      return false;

   if (msg.destId() == Id::ForBroadcast)
      return true;

   const int t = msg.type();
   if (rt[t] & DestMasterHub) {
      if (senderType != Identify::HUB)
         if (m_identity != Identify::HUB)
            return true;
   }

   if (rt[t] & DestSlaveHub) {
      return true;
   }

   if (rt[t] & Broadcast) {
      if (msg.senderId() == m_id)
         return true;
      std::cerr << "toMasterHub: sender id: " << msg.senderId() << ", hub: " << senderHub << std::endl;
      if (senderHub == m_id)
         return true;
   }

   return false;
}

bool Router::toSlaveHub(const Message &msg, Identify::Identity senderType, int senderHub) {

   if (msg.destId() == Id::ForBroadcast)
      return false;

   if (m_identity != Identify::HUB)
      return false;

   if (msg.destId() == Id::Broadcast) {
      return true;
   }

   const int t = msg.type();
   if (rt[t] & DestSlaveHub) {
      return true;
   }

#if 0
   if (m_identity == Identify::SLAVEHUB) {
      if (rt[t] & DestManager)
         return true;
   } else if (m_identity == Identify::HUB) {
      if (rt[t] & DestManager)
         return true;
   }
#endif

   if (rt[t] & Broadcast)
      return true;

   return false;
}

bool Router::toManager(const Message &msg, Identify::Identity senderType) {

   const int t = msg.type();
   if (msg.destId() <= Id::MasterHub) {
      if (msg.destId() == m_id && rt[t] & DestLocalManager) {
         return true;
      }
      return false;
   }
   if (senderType != Identify::MANAGER) {
      if (rt[t] & DestManager)
         return true;
      if  (rt[t] & DestModules)
         return true;
      if (rt[t] & Broadcast)
         return true;
   }

   return false;
}

bool Router::toModule(const Message &msg, Identify::Identity senderType) {

   if (msg.destId() == Id::ForBroadcast)
      return false;

   const int t = msg.type();
   if (rt[t] & DestModules)
      return true;
   if (rt[t] & Broadcast)
      return true;

   return false;
}

bool Router::toTracker(const Message &msg, Identify::Identity senderType) {

   if (msg.destId() == Id::ForBroadcast)
      return false;

   const int t = msg.type();
   if (rt[t] & Track) {
      if (m_identity == Identify::HUB) {
         return senderType == Identify::SLAVEHUB || senderType == Identify::MANAGER;
      }
      if (m_identity == Identify::SLAVEHUB) {
         return senderType == Identify::HUB || senderType == Identify::MANAGER;
      }
   }

   return false;
}

bool Router::toHandler(const Message &msg, Identify::Identity senderType) {

   const int t = msg.type();
   if (msg.destId() == Id::NextHop || msg.destId() == Id::Broadcast || msg.destId() == m_id) {
      return true;
   }
   if (m_identity == Identify::HUB) {
      if (rt[t] & HandleOnMaster) {
         return true;
      }
      if (rt[t] & DestMasterHub)
         return true;
   }
   if (m_identity == Identify::SLAVEHUB || m_identity == Identify::HUB) {
      if (rt[t] & HandleOnHub)
         return true;
      if (rt[t] & DestLocalHub)
         return true;
   }
   if (m_identity == Identify::SLAVEHUB) {
      if (rt[t] & DestSlaveHub)
         return true;
   }
   if (m_identity == Identify::MANAGER) {
      if (m_id == Id::MasterHub)
         return rt[t] & DestMasterManager;
      else
         return rt[t] & DestSlaveManager;
   }

   return false;
}

} // namespace message
} // namespace vistle
