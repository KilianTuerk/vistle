#include <vistle/util/boost_interprocess_config.h>
#include <boost/interprocess/exceptions.hpp>

// cover
#include <net/message.h>
#include <cover/coVRPluginSupport.h>
#include <cover/coCommandLine.h>
#include <cover/OpenCOVER.h>
#include <PluginUtil/PluginMessageTypes.h>

#include <VistlePluginUtil/VistleMessage.h>

#include <COVER.h>

// vistle
#include <vistle/util/exception.h>
#include <vistle/core/statetracker.h>
#include <vistle/control/vistleurl.h>

#include <osgGA/GUIEventHandler>
#include <osgViewer/Viewer>

#include <cover/ui/Owner.h>
#include <cover/ui/Menu.h>
#include <cover/ui/Action.h>

using namespace opencover;
using namespace vistle;

class VistlePlugin: public opencover::coVRPlugin, public ui::Owner {
public:
    VistlePlugin();
    ~VistlePlugin() override;
    bool init() override;
    bool destroy() override;
    void notify(NotificationLevel level, const char *text) override;
    bool update() override;
    void requestQuit(bool killSession) override;
    bool executeAll() override;
    void message(int toWhom, int type, int length, const void *data) override;
    bool sendVisMessage(const covise::Message *msg) override;
    std::string collaborativeSessionId() const override;

    void updateSessionUrl(const std::string &url);

private:
    COVER *m_module = nullptr;
    std::unique_ptr<vistle::StateObserver> m_observer;
};


class CoverVistleObserver: public vistle::StateObserver {
    VistlePlugin *m_plug = nullptr;

public:
    CoverVistleObserver(VistlePlugin *plug): m_plug(plug) {}
    void sessionUrlChanged(const std::string &url) override { m_plug->updateSessionUrl(url); }
};

void VistlePlugin::updateSessionUrl(const std::string &url)
{
    vistle::ConnectionData cd;
    if (VistleUrl::parse(url, cd)) {
        std::cerr << "session URL changed: " << url << std::endl;
        if (!cd.conference_url.empty()) {
            cover->addPlugin("Browser");
            cover->sendMessage(this, coVRPluginSupport::TO_ALL, opencover::PluginMessageTypes::Browser,
                               cd.conference_url.length(), cd.conference_url.c_str());
        }
    }
}


VistlePlugin::VistlePlugin(): ui::Owner("Vistle", cover->ui), m_module(nullptr)
{
#if 0
    using opencover::coCommandLine;

   int initialized = 0;
   MPI_Initialized(&initialized);
   if (!initialized) {
      std::cerr << "VistlePlugin: no MPI support - started from within Vistle?" << std::endl;
      //throw(vistle::exception("no MPI support"));
      return;
   }

   if (coCommandLine::argc() < 3) {
      for (int i=0; i<coCommandLine::argc(); ++i) {
         std::cout << i << ": " << coCommandLine::argv(i) << std::endl;
      }
      throw(vistle::exception("at least 2 command line arguments required"));
   }

   int rank, size;
   MPI_Comm_rank(MPI_COMM_WORLD, &rank);
   MPI_Comm_size(MPI_COMM_WORLD, &size);
   const std::string &shmname = coCommandLine::argv(1);
   const std::string &name = coCommandLine::argv(2);
   int moduleID = atoi(coCommandLine::argv(3));

   vistle::Module::setup(shmname, moduleID, rank);
   m_module = new OsgRenderer(shmname, name, moduleID);
#endif
}

VistlePlugin::~VistlePlugin()
{
    if (m_module) {
        m_module->comm().barrier();
        m_module->prepareQuit();
#if 0
      delete m_module;
#endif
        m_module = nullptr;
    }
}

bool VistlePlugin::init()
{
    assert(!cover->visMenu);

    m_module = COVER::the();
    m_observer = std::make_unique<CoverVistleObserver>(this);

    if (m_module) {
        m_module->setPlugin(this);

        m_module->state().registerObserver(m_observer.get());
        updateSessionUrl(m_module->state().sessionUrl());

        cover->visMenu = new ui::Menu("Vistle", this);

        auto executeButton = new ui::Action("Execute", cover->visMenu);
        cover->visMenu->add(executeButton, ui::Container::KeepFirst);
        executeButton->setShortcut("e");
        executeButton->setCallback([this]() { executeAll(); });
        executeButton->setIcon("view-refresh");
        executeButton->setPriority(ui::Element::Toolbar);

        update();

        return true;
    }

    return false;
}

bool VistlePlugin::destroy()
{
    if (m_module) {
        m_module->state().unregisterObserver(m_observer.get());
        m_module->comm().barrier();
        m_module->prepareQuit();
#if 0
      delete m_module;
#endif
        m_module = nullptr;
    }

    m_observer.reset();

    return true;
}

void VistlePlugin::notify(coVRPlugin::NotificationLevel level, const char *message)
{
    std::vector<char> text(strlen(message) + 1);
    memcpy(&text[0], message, text.size());
    if (text.size() > 1 && text[text.size() - 2] == '\n')
        text[text.size() - 2] = '\0';
    if (text[0] == '\0')
        return;
    std::cerr << &text[0] << std::endl;
    if (m_module) {
        switch (level) {
        case coVRPlugin::Info:
            m_module->sendInfo("%s", &text[0]);
            break;
        case coVRPlugin::Warning:
            m_module->sendWarning("%s", &text[0]);
            break;
        case coVRPlugin::Error:
        case coVRPlugin::Fatal:
            m_module->sendError("%s", &text[0]);
            break;
        }
    }
}

bool VistlePlugin::update()
{
#ifndef NDEBUG
    if (m_module) {
        m_module->comm().barrier();
    }
#endif
    bool messageReceived = false;
    try {
        if (m_module && !m_module->dispatch(false, &messageReceived)) {
            std::cerr << "Vistle requested COVER to quit" << std::endl;
            OpenCOVER::instance()->requestQuit();
        }
    } catch (boost::interprocess::interprocess_exception &e) {
        std::cerr << "Module::dispatch: interprocess_exception: " << e.what() << std::endl;
        std::cerr << "   error: code: " << e.get_error_code() << ", native: " << e.get_native_error() << std::endl;
        throw(e);
    } catch (std::exception &e) {
        std::cerr << "Module::dispatch: std::exception: " << e.what() << std::endl;
        throw(e);
    }

    bool updateRequested = false;
    if (m_module) {
        updateRequested = m_module->updateRequired();
        m_module->clearUpdate();
    }
    return messageReceived || updateRequested;
}

void VistlePlugin::requestQuit(bool killSession)
{
    if (m_module) {
        m_module->comm().barrier();
        m_module->prepareQuit();
#if 0
        delete m_module;
#endif
        m_module = nullptr;
    }
}

bool VistlePlugin::executeAll()
{
    if (m_module) {
        return m_module->executeAll();
    }
    return false;
}

void VistlePlugin::message(int toWhom, int type, int length, const void *data)
{
    if (type == opencover::PluginMessageTypes::VistleMessageOut) {
        const auto *wrap = static_cast<const VistleMessage *>(data);
        if (m_module) {
            m_module->sendMessage(wrap->buf, wrap->payload);
        }
    }
}

bool VistlePlugin::sendVisMessage(const covise::Message *msg)
{
    if (!m_module) {
        return false;
    }

    message::Cover cover(m_module->mirrorId(), msg->sender, msg->send_type, msg->type);
    cover.setDestId(message::Id::MasterHub);
    MessagePayload pl(msg->data.data(), msg->data.length());
    cover.setPayloadSize(msg->data.length());
    return m_module->sendMessage(cover, pl);
}

std::string VistlePlugin::collaborativeSessionId() const
{
    std::string id = "COVER";
    if (!m_module) {
        return id;
    }

    id += "_" + std::to_string(m_module->mirrorId());
    return id;
}

COVERPLUGIN(VistlePlugin)
