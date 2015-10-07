#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtCore/QtGlobal>
#include <QPointer>
#include <QMainWindow>
#include <QToolBar>
#include <QProgressBar>
#include <QSettings>
#include <QLabel>
#include <qlist.h>
#include <cstring>
#include <QProcess>

#include "common/common.h"
#include "common/mavlink.h"
#include "mavlink_types.h"
#include "ui_mainwindow.h"
#include "linkinterface.h"
#include "uasinterface.h"
#include "mavlinkdecoder.h"
#include "commconfigurationwindow.h"


namespace Ui {
class MainWindow;
}
class Drone;
class Window;
class GLWidget;
class ToolBar;
class MAVLinkProtocol;
class MAVLinkDecoder;
class UAVConfig;
class UASInterface;
class SerialLink;
class MAVLinkMessageSender;
class ParameterInterface;
class UASInfoWidget;
class CommConfigurationWindow;
class HUDWidget;
class CompassWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    ///   Returns the MainWindow singleton. Will not create the MainWindow if it has not already
    ///         been created.
    static MainWindow* instance(void);

    QList<QAction*> listLinkMenuActions(void);
    QAction *getActionByLink(LinkInterface *link);


protected:
    bool connectFlag;
    MAVLinkProtocol* mavlink;
    UAVConfig *config; //main tab configuration VSK
    UASInterface* _uas;
    QTextEdit* activeProcessStatusWdgt;
    bool fwFlashActive;
    QProcess ps_master;
    QString fileToFlash;
    const char *platformExeExt; // OS-specific executables suffix (.exe for Win)
    QString aqBinFolderPath;    // absolute path to AQ supporting utils

public slots:
    /**   Add a communication link */
    void addLink();
    void addLink(LinkInterface* link);
    void addLinkImmediately();

    /**   Add a new UAS */
    void UASCreated(UASInterface* uas);
    /**   Set the system that is currently displayed by this widget */
    void setActiveUAS(UASInterface* active);
    /**   Update system specs of a UAS */
    void UASSpecsChanged(int uas);
    /** Delete an UAS */
    void UASDeleted(UASInterface* uas);

    void closeSerialPort();


    bool checkAqConnected(bool interactive = false);
    void selectFWToFlash();
    void flashFW();
    void flashFwDfu();
    bool checkProcRunning(bool warn = true);
    void prtstexit(int stat);
    void prtstdout();
    QString extProcessError(QProcess::ProcessError err);

    /**   Shows an info or warning message */
    void showMessage(const QString &title, const QString &message, const QString &details, const QString severity = "info");
    /**   Shows a critical message as popup or as widget */
    void showCriticalMessage(const QString& title, const QString& message);


public:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
