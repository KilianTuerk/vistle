#ifndef UICONTROLLER_H
#define UICONTROLLER_H

#include <QObject>

#include <vistle/userinterface/vistleconnection.h>
#include "vistleobserver.h"
#include "mainwindow.h"

#include <thread>

namespace vistle {
class PythonInterface;
struct PythonStateAccessor;
class PythonModule;
} // namespace vistle

namespace gui {

class MainWindow;

class UiController: public QObject {
    Q_OBJECT

public:
    explicit UiController(int argc, char *argv[], QObject *parent = nullptr);
    ~UiController();
    void init();
    void finish();

signals:

public slots:
    void quitRequested(bool &allowed);

    void aboutVistle();
    void aboutLicense();
    void aboutIcons();
    void aboutQt();

private slots:
    void setModified(bool mod);
    bool checkModified(const QString &reason);
    void clearDataFlowNetwork();
    void loadDataFlowNetwork();
    void saveDataFlowNetwork(const QString &filename = QString());
    void saveDataFlowNetworkAs(const QString &filename = QString());
    void executeDataFlowNetwork();
    void connectVistle();

    void moduleSelectionChanged();

    void newParameter(int moduleId, QString parameterName);
    void parameterValueChanged(int moduleId, QString parameterName);

    void statusUpdated(int id, QString text, int prio);
    void setCurrentFile(QString file);
    void setSessionUrl(QString url);

    void showConnectionInfo();
    void copyConnectionInfo();
    void screenshot(QString imageefile, bool quit);

private:
    void about(const char *title, const char *file);

    std::unique_ptr<vistle::VistleConnection> m_vistleConnection;
    std::unique_ptr<vistle::UserInterface> m_ui;
    std::unique_ptr<vistle::PythonInterface> m_python;
    std::unique_ptr<vistle::PythonStateAccessor> m_pythonAccess;
    std::unique_ptr<vistle::PythonModule> m_pythonMod;
    std::unique_ptr<std::thread> m_thread;
    DataFlowNetwork *m_scene = nullptr;

    VistleObserver m_observer;
    MainWindow *m_mainWindow = nullptr;

    QString m_currentFile;
    bool m_modified = false;
    std::string m_pythonDir;

    QString m_sessionUrl;
};

} // namespace gui
#endif // UICONTROLLER_H
