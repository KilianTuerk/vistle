/*********************************************************************************/
/** \file port.cpp
 *
 * graphical representation for a module port
 */
/**********************************************************************************/
#include "port.h"
#include "module.h"
#include "dataflownetwork.h"
#include <vistle/core/port.h>
#include <cassert>

#include <QCursor>
#include <QAction>
#include <QMenu>
#include <QRadialGradient>
#include <QPainter>
#include <QGraphicsSceneMouseEvent>

namespace gui {

const double Port::portSize = 14.;

static QColor InColor(200, 30, 30);
static QColor OutColor(200, 200, 30);
static QColor ParamColor(30, 30, 200);

Port::Port(const vistle::Port *port, Module *parent)
: Base(parent)
, m_port(port)
, m_module(parent)
, m_moduleId(port->getModuleID())
, m_name(QString::fromStdString(port->getName()))
{
    switch (port->getType()) {
    case vistle::Port::INPUT:
        m_portType = Input;
        break;
    case vistle::Port::OUTPUT:
        m_portType = Output;
        break;
    case vistle::Port::PARAMETER:
        m_portType = Parameter;
        break;
    default:
        assert("port type not handled" == 0);
        break;
    }

    createGeometry();
    createMenus();
    createTooltip();
}

void Port::createMenus()
{
    connect(this, &Port::selectConnected, m_module, &Module::selectConnected);

    m_portMenu = new QMenu();

    if (m_portType == Input) {
        m_selectUpstreamAct = new QAction("Select Upstream", this);
        m_selectUpstreamAct->setStatusTip("Select all modules feeding data to this port");
        connect(m_selectUpstreamAct, &QAction::triggered,
                [this]() { emit selectConnected(SelectUpstream, m_moduleId, m_name); });
        m_portMenu->addAction(m_selectUpstreamAct);
    }

    m_selectConnectedAct = new QAction("Select Connected", this);
    m_selectConnectedAct->setStatusTip("Select all modules connected to this port");
    connect(m_selectConnectedAct, &QAction::triggered,
            [this]() { emit selectConnected(SelectConnected, m_moduleId, m_name); });
    m_portMenu->addAction(m_selectConnectedAct);

    if (m_portType == Output) {
        m_selectDownstreamAct = new QAction("Select Downstream", this);
        m_selectDownstreamAct->setStatusTip("Select all modules this port feeds into");
        connect(m_selectDownstreamAct, &QAction::triggered,
                [this]() { emit selectConnected(SelectDownstream, m_moduleId, m_name); });
        m_portMenu->addAction(m_selectDownstreamAct);
    }

    m_portMenu->addSeparator();

    m_disconnectAct = new QAction("Remove Connections", this);
    m_disconnectAct->setStatusTip("Remove connections to all other modules");
    connect(m_disconnectAct, &QAction::triggered, [this]() {
        auto *sc = dynamic_cast<DataFlowNetwork *>(scene());
        assert(sc);
        sc->removeConnections(this, true);
    });
    m_portMenu->addAction(m_disconnectAct);
}

void Port::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    m_portMenu->popup(event->screenPos());
}

QString Port::name() const
{
    return m_name;
}

bool Port::valid() const
{
    return m_port != nullptr;
}

const vistle::Port *Port::vistlePort() const
{
    return m_port;
}

bool Port::operator<(const Port &other) const
{
    if (m_moduleId == other.m_moduleId) {
        return m_name < other.m_name;
    }
    return m_moduleId < other.m_moduleId;
}

bool Port::operator==(const Port &other) const
{
    return m_moduleId == other.m_moduleId && m_name == other.m_name;
}

Port::Type Port::portType() const
{
    return m_portType;
}

Module *Port::module() const
{
    return m_module;
}

void Port::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    // ball
    QRadialGradient gradient(4, 4, portSize / 2);
    gradient.setCenter(6, 6);
    gradient.setFocalPoint(6, 6);

    //painter->setPen(QPen(Qt::black, 0));
    painter->setPen(Qt::NoPen);
    gradient.setColorAt(0, Qt::white);
    gradient.setColorAt(0, m_color);
    gradient.setColorAt(1, m_color);
    painter->setBrush(gradient);
    painter->drawRect(0, 0, portSize, portSize);
}

QPointF Port::scenePos() const
{
    return mapToScene(mapFromItem(m_module, m_module->portPos(this)));
}

void Port::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    auto *sc = dynamic_cast<DataFlowNetwork *>(scene());
    assert(sc);
    sc->setConnectionHighlights(this, true);
}

void Port::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    auto *sc = dynamic_cast<DataFlowNetwork *>(scene());
    assert(sc);
    sc->setConnectionHighlights(this, false);
}

void Port::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    Base::mousePressEvent(event);
}

void Port::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
#if 0
   if (rect().contains(event->pos())) {
      emit clicked(this);
      return;
   }
#endif

    DataFlowNetwork *sc = dynamic_cast<DataFlowNetwork *>(scene());
    QGraphicsItem *item = scene()->itemAt(event->scenePos(), QTransform());
    if (Port *dest = dynamic_cast<Port *>(item)) {
        if (portType() == Output && dest->portType() == Input) {
            sc->addConnection(this, dest, true);
        } else if (portType() == Input && dest->portType() == Output) {
            sc->addConnection(dest, this, true);
        }
    }

    Base::mouseReleaseEvent(event);
}

void Port::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    Base::mouseMoveEvent(event);
}

void Port::createTooltip()
{
    QString toolTip = "Invalid!";

    if (m_port) {
        QString desc = QString::fromStdString(m_port->getDescription());
        QString name = QString::fromStdString(m_port->getName());
        toolTip = desc;
        if (!toolTip.isEmpty()) {
            toolTip += " ";
        }
        toolTip += "(" + name + ")";
    }

    setToolTip(toolTip);
}

void Port::createGeometry()
{
    setCursor(Qt::CrossCursor);
    setAcceptHoverEvents(true);

    switch (m_portType) {
    case Port::Input:
        m_color = InColor;
        break;
    case Port::Output:
        m_color = OutColor;
        break;
    case Port::Parameter:
        m_color = ParamColor;
        break;
    }

    setRect(0., 0., portSize, portSize);
}

} //namespace gui
