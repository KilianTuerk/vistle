#include "dataflowview.h"
#include "modulebrowser.h"
#include "dataflownetwork.h"
#include "module.h"

#include <QMenu>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>

namespace gui {

DataFlowView *DataFlowView::s_instance = nullptr;

DataFlowView::DataFlowView(QWidget *parent): QGraphicsView(parent)
{
    if (!s_instance)
        s_instance = this;

    createActions();
    createMenu();

    setAttribute(Qt::WA_AlwaysShowToolTips);
    setDragMode(QGraphicsView::RubberBandDrag);
    setMouseTracking(true);
    setTransformationAnchor(QGraphicsView::NoAnchor);

    if (scene())
        connect(scene(), SIGNAL(selectionChanged()), this, SLOT(enableActions()));
}

DataFlowView::~DataFlowView()
{
    if (scene())
        disconnect(scene(), SIGNAL(selectionChanged()), this, SLOT(enableActions()));
}

DataFlowView *DataFlowView::the()
{
    if (!s_instance)
        s_instance = new DataFlowView();

    return s_instance;
}


DataFlowNetwork *DataFlowView::scene() const
{
    return dynamic_cast<DataFlowNetwork *>(QGraphicsView::scene());
}

/*!
 * \brief DataFlowView::dragEnterEvent
 * \param event
 *
 * \todo clean up the distribution of event handling.
 */
void DataFlowView::dragEnterEvent(QDragEnterEvent *e)
{
    const QMimeData *mimeData = e->mimeData();
    const QStringList &mimeFormats = mimeData->formats();
    if (mimeFormats.contains(ModuleBrowser::mimeFormat())) {
        e->acceptProposedAction();
    }
}

void DataFlowView::dragMoveEvent(QDragMoveEvent *event)
{
    event->accept();
}

/*!
 * \brief DataFlowView::dropEvent takes action on a drop event anywhere in the window.
 *
 * This drop event method is currently the only drop event in the program. It looks for drops into the
 * QGraphicsView (drawArea), and calls the scene to add a module depending on the position.
 *
 * \param event
 * \todo clarify all the event handling, and program the creation and handling of events more elegantly.
 * \todo put information into the event, to remove the need to have drag events in the mainWindow
 */
void DataFlowView::dropEvent(QDropEvent *event)
{
    QPointF newPos = mapToScene(event->pos());

    if (event->mimeData()->formats().contains(ModuleBrowser::mimeFormat())) {
        QByteArray encoded = event->mimeData()->data(ModuleBrowser::mimeFormat());
        QDataStream stream(&encoded, QIODevice::ReadOnly);

        while (!stream.atEnd()) {
            int hubId;
            stream >> hubId;
            QString moduleName;
            stream >> moduleName;
            scene()->addModule(hubId, moduleName, newPos);
        }
    }
}

void DataFlowView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        const ViewportAnchor ranchor = resizeAnchor();
        const ViewportAnchor tanchor = transformationAnchor();
        setResizeAnchor(QGraphicsView::AnchorUnderMouse);
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
        int angle = event->angleDelta().y();
        qreal factor;
        if (angle > 0) {
            factor = 1.0003;
        } else {
            factor = 1.0 / 1.0003;
        }
        qreal f = 1.0;
        for (int i = 0; i < abs(angle); ++i)
            f *= factor;
        scale(f, f);
        setResizeAnchor(ranchor);
        setTransformationAnchor(tanchor);
    } else {
        QGraphicsView::wheelEvent(event);
    }
}

void DataFlowView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        m_isPanning = true;
        m_panPos = event->pos();
        m_savedCursor = viewport()->cursor();
        viewport()->setCursor(Qt::ClosedHandCursor);
        event->accept();
    } else {
        QGraphicsView::mousePressEvent(event);
    }
}

void DataFlowView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isPanning) {
        event->accept();
        QPointF oldp = mapToScene(m_panPos);
        QPointF newp = mapToScene(event->pos());
        m_panPos = event->pos();
        QPointF translation = newp - oldp;

        translate(translation.x(), translation.y());
    } else {
        QGraphicsView::mouseMoveEvent(event);
    }
}

void DataFlowView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_isPanning) {
        m_isPanning = false;
        viewport()->setCursor(m_savedCursor);
        event->accept();
    } else {
        QGraphicsView::mouseReleaseEvent(event);
    }
}

void DataFlowView::createActions()
{
    m_deleteAct = new QAction("Delete Selected", this);
    m_deleteAct->setShortcuts(QKeySequence::Delete);
    m_deleteAct->setShortcutContext(Qt::ApplicationShortcut);
    m_deleteAct->setStatusTip("Delete the selected modules and all their connections");
    connect(m_deleteAct, SIGNAL(triggered()), this, SLOT(deleteModules()));

    m_execAct = new QAction("Execute Workflow", this);
    m_execAct->setStatusTip("Execute the workflow");
    connect(m_execAct, SIGNAL(triggered()), this, SLOT(execModules()));

    m_selectSourcesAct = new QAction("Select Sources", this);
    m_selectSourcesAct->setStatusTip("Select all source modules");
    connect(m_selectSourcesAct, SIGNAL(triggered()), this, SLOT(selectSourceModules()));

    m_selectSinksAct = new QAction("Select Sinks", this);
    m_selectSinksAct->setStatusTip("Select all sink modules");
    connect(m_selectSinksAct, SIGNAL(triggered()), this, SLOT(selectSinkModules()));

    m_selectInvertAct = new QAction("Invert Selection", this);
    m_selectInvertAct->setStatusTip("Toggle selection state of modules");
    connect(m_selectInvertAct, SIGNAL(triggered()), this, SLOT(selectInvert()));
}

void DataFlowView::enableActions()
{
    if (selectedModules().empty()) {
        m_deleteAct->setEnabled(false);
    } else {
        m_deleteAct->setEnabled(true);
    }
}

void DataFlowView::createMenu()
{
    m_contextMenu = new QMenu();
    m_contextMenu->addAction(m_execAct);
    m_contextMenu->addSeparator();
    m_contextMenu->addAction(m_selectSourcesAct);
    m_contextMenu->addAction(m_selectSinksAct);
    m_contextMenu->addAction(m_selectInvertAct);
    m_contextMenu->addSeparator();
    m_contextMenu->addAction(m_deleteAct);
}

void DataFlowView::contextMenuEvent(QContextMenuEvent *event)
{
    if (itemAt(event->pos())) {
        QGraphicsView::contextMenuEvent(event);
        return;
    }

    m_contextMenu->popup(event->globalPos());
}

QList<Module *> DataFlowView::selectedModules()
{
    QList<Module *> list;
    if (scene()) {
        for (auto &item: scene()->selectedItems()) {
            if (auto module = dynamic_cast<Module *>(item))
                list.append(module);
        }
    }
    return list;
}

void DataFlowView::setScene(QGraphicsScene *s)
{
    if (scene())
        disconnect(scene(), SIGNAL(selectionChanged()), this, SLOT(enableActions()));
    QGraphicsView::setScene(s);
    if (scene())
        connect(scene(), SIGNAL(selectionChanged()), this, SLOT(enableActions()));
    enableActions();
}

void DataFlowView::execModules()
{
    auto selection = selectedModules();
    if (selection.empty()) {
        emit executeDataFlow();
    } else {
        for (auto m: selection)
            m->execModule();
    }
}

void DataFlowView::cancelExecModules()
{
    for (auto &m: selectedModules())
        m->cancelExecModule();
}

/*!
 * \brief Module::deleteModule
 */
void DataFlowView::deleteModules()
{
    for (auto &m: selectedModules())
        m->deleteModule();
}

void DataFlowView::selectAllModules()
{
    if (scene()) {
        for (auto &item: scene()->items()) {
            if (auto module = dynamic_cast<Module *>(item))
                module->setSelected(true);
        }
    }
}

void DataFlowView::selectSourceModules()
{
    if (!scene())
        return;

    for (auto &item: scene()->items()) {
        if (auto module = dynamic_cast<Module *>(item)) {
            bool select = true;
            for (auto *p: module->inputPorts()) {
                auto *vp = module->getVistlePort(p);
                if (vp->isConnected()) {
                    select = false;
                    break;
                }
            }
            if (select)
                module->setSelected(true);
        }
    }
}

void DataFlowView::selectSinkModules()
{
    if (!scene())
        return;

    for (auto &item: scene()->items()) {
        if (auto module = dynamic_cast<Module *>(item)) {
            bool select = true;
            for (auto *p: module->outputPorts()) {
                auto *vp = module->getVistlePort(p);
                if (vp->isConnected()) {
                    select = false;
                    break;
                }
            }
            if (select)
                module->setSelected(true);
        }
    }
}

void DataFlowView::selectClear()
{
    if (!scene())
        return;

    for (auto &item: scene()->items()) {
        if (auto module = dynamic_cast<Module *>(item)) {
            module->setSelected(false);
        }
    }
}

void DataFlowView::selectInvert()
{
    if (!scene())
        return;

    for (auto &item: scene()->items()) {
        if (auto module = dynamic_cast<Module *>(item)) {
            module->setSelected(!module->isSelected());
        }
    }
}

void DataFlowView::zoomOrig()
{
    setTransform(QTransform());
}

void DataFlowView::zoomAll()
{
    fitInView(scene()->itemsBoundingRect(), Qt::KeepAspectRatio);
}

bool DataFlowView::snapshot(const QString &filename)
{
    //scene()->itemsBoundingRect() does not return a correct boundig box
    scene()->setSceneRect(scene()->calculateBoundingBox()); // Re-shrink the scene to it's bounding contents

    QImage image(scene()->sceneRect().size().toSize() * 2,
                 QImage::Format_ARGB32); // Create the image with the exact size of the shrunk scene
    image.fill(Qt::transparent); // Start all pixels transparent

    QPainter painter(&image);
    scene()->render(&painter);
    QString msg;
    if (image.save(filename)) {
        msg = QString("Picture was stored as %1 (%2x%3 pixels)")
                  .arg(filename, QString::number(image.width()), QString::number(image.height()));
        std::cerr << msg.toStdString() << std::endl;
        return true;
    } else {
        msg = "Failed to store picture as " + filename;
        std::cerr << msg.toStdString() << std::endl;
        return false;
    }
}

} // namespace gui
