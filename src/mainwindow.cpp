#include <QWidget>
#include <QMainWindow>
#include <QMessageBox>
#include <QtSerialPort/QSerialPort>
#include <QTimer>
#include <QDebug>
#include <QLayout>
#include <QWidget>
#include <QFile>
#include <QTextStream>
#include <QDockWidget>
#include <QSettings>
#include <QDesktopWidget>
#include <QStyleFactory>
#include <QDesktopServices>
#include <QVariant>
#include <QToolBar>
#include <QFileInfo>
#include <QFileDialog>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "qgc.h"
#include "uasmanager.h"
#include "linkmanager.h"
#include "seriallink.h"
#include "mavlinkprotocol.h"

static MainWindow* _instance = NULL;   //MainWindow singleton

MainWindow* MainWindow::instance(void)
{
    return _instance;
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    connectFlag(true),
    _uas(NULL),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    aqBinFolderPath = QCoreApplication::applicationDirPath() + "/aq/bin/";
    platformExeExt = ".exe";

    // Populate link menu
    QList<LinkInterface*> links = LinkManager::instance()->getLinks();
    foreach(LinkInterface* link, links)
    {
        this->addLink(link);
    }
    connect(LinkManager::instance(), SIGNAL(newLink(LinkInterface*)), this, SLOT(addLink(LinkInterface*)));

    //Move protocol outside UI
    mavlink = new MAVLinkProtocol();

    addLink();

    connect(UASManager::instance(), SIGNAL(UASCreated(UASInterface*)), this, SLOT(UASCreated(UASInterface*)));
    connect(UASManager::instance(), SIGNAL(UASDeleted(UASInterface*)), this, SLOT(UASDeleted(UASInterface*)));


    connect(ui->flashButton, SIGNAL(clicked()), this, SLOT(flashFW()));
    connect(ui->SelectFirmwareButton, SIGNAL(clicked()), this, SLOT(selectFWToFlash()));


    //Process Slots for Update Firmware
    ps_master.setProcessChannelMode(QProcess::MergedChannels);
    connect(&ps_master, SIGNAL(finished(int)), this, SLOT(prtstexit(int)));
    connect(&ps_master, SIGNAL(readyReadStandardOutput()), this, SLOT(prtstdout()));
    connect(&ps_master, SIGNAL(error(QProcess::ProcessError)), this, SLOT(extProcessError(QProcess::ProcessError)));

    setActiveUAS(UASManager::instance()->getActiveUAS());
    connect(UASManager::instance(), SIGNAL(activeUASSet(UASInterface*)), this, SLOT(setActiveUAS(UASInterface*)));
//    updateGUI(false);
}

MainWindow::~MainWindow()
{
    foreach(LinkInterface* link,LinkManager::instance()->getLinks())
    {
        link->disconnect();
        LinkManager::instance()->removeLink(link);
    }
    delete ui;
}

void MainWindow::addLinkImmediately()
{
    if (connectFlag)
    {
        SerialLink *link = new SerialLink();
        CommConfigurationWindow* commWidget = new CommConfigurationWindow(link, mavlink, this);

        commWidget->setConnection();

        LinkManager::instance()->add(link);
        LinkManager::instance()->addProtocol(link, mavlink);

        if (link->isPortHandleValid())
        {
            if (link->connect())
            {
                //                ui->btn_connectBoard->setText(QString("Disconnect"));
                //                ui->scrollArea->setStyleSheet(QString("background-color: green; border-radius: 12px"));
                //                ui->statusBar->showMessage(tr("Connected"));
                updateGUI(true);
                ui->actionConfigure->setEnabled(false);
                connectFlag = false;
            }
        }
        else
        {
            LinkManager::instance()->removeLink(link);
            MainWindow::instance()->showCriticalMessage(tr("Error!"), tr("Please plugin your device to begin."));
        }
    }
    else
    {
        closeSerialPort();
        connectFlag = true;
    }
}

void MainWindow::addLink()
{
    SerialLink* link = new SerialLink();
    LinkManager::instance()->add(link);
    LinkManager::instance()->addProtocol(link, mavlink);

    QAction* act = getActionByLink(link);
    if (act)
        act->trigger();
}

void MainWindow::addLink(LinkInterface *link)
{
    LinkManager::instance()->add(link);
    LinkManager::instance()->addProtocol(link, mavlink);

    if (!getActionByLink(link))
    {
        CommConfigurationWindow* commWidget = new CommConfigurationWindow(link, mavlink, this);
        ui->scrollArea_seriallink->setWidget(commWidget);
        connect(link, SIGNAL(communicationError(QString,QString)), this, SLOT(showCriticalMessage(QString,QString)), Qt::QueuedConnection);

    }
}

void MainWindow::setActiveUAS(UASInterface *uas)
{
    // Do nothing if system is the same or NULL
    if (uas == NULL) return;

//    connect(uas,SIGNAL(heartbeatTimeout(bool,uint)),this,SLOT(heartbeatTimeout(bool,uint)));
}

void MainWindow::UASSpecsChanged(int uas)
{
    UASInterface* activeUAS = UASManager::instance()->getActiveUAS();
    if (activeUAS)
    {
        if (activeUAS->getUASID() == uas)
        {
            ui->menuWidgets->setTitle(activeUAS->getUASName());
        }
    }
    else
    {
        // Last system deleted
        ui->menuWidgets->setTitle(tr("No System"));
        ui->menuWidgets->setEnabled(false);
    }
}

void MainWindow::UASCreated(UASInterface* uas)
{
    this->_uas = uas;
    if (!_uas)
        return;

    connect(_uas, SIGNAL(systemSpecsChanged(int)), this, SLOT(UASSpecsChanged(int)));

    updateGUI(true);

}

void MainWindow::UASDeleted(UASInterface *uas)
{
    QAction* act;
    QList<QAction*> actions = ui->menuWidgets->actions();
    foreach (act, actions) {
        if (act->text().contains(uas->getUASName()))
            ui->menuWidgets->removeAction(act);
    }
}

void MainWindow::closeSerialPort()
{
    foreach(LinkInterface* link,LinkManager::instance()->getLinks())
    {
        link->disconnect();
        LinkManager::instance()->removeLink(link);
        updateGUI(false);

    }
    ui->actionConfigure->setEnabled(true);
}

QList<QAction *> MainWindow::listLinkMenuActions()
{
    return ui->menuWidgets->actions();
}

QAction *MainWindow::getActionByLink(LinkInterface *link)
{
    QAction *ret = NULL;
    int linkIndex = LinkManager::instance()->getLinks().indexOf(link);
    int linkID = LinkManager::instance()->getLinks().at(linkIndex)->getId();

    foreach (QAction* act, listLinkMenuActions())
    {
        if (act->data().toInt() == linkID)
            return act;
    }
    return ret;
}

/*
 * Update FW
 */
bool MainWindow::checkAqConnected(bool interactive)
{
    if (!_uas || _uas->getCommunicationStatus() != _uas->COMM_CONNECTED ) {
        if (interactive){
            showCriticalMessage("Error", "No AutoQuad connected!");
        }
        return false;
    } else
        return true;
}

void MainWindow::selectFWToFlash()
{
    QString dirPath;
    dirPath = QCoreApplication::applicationDirPath();

    QFileInfo dir(dirPath);

    QString fileName = QFileDialog::getOpenFileName(this, tr("Select Firmware File"), dir.absoluteFilePath(),
                                                    tr("AQ firmware") + " (*.hex *.bin)");

    if (fileName.length())
    {
        QString fileNameLocale = QDir::toNativeSeparators(fileName);
        QFile file(fileNameLocale);
        if (!file.open(QIODevice::ReadOnly))
        {
            qDebug() << "could not open";
            showCriticalMessage(tr("Warning!"),tr("Could not open firmware file. %1").arg(file.errorString()));
            return;
        }
        ui->fileLabel->setText(fileNameLocale);
        ui->fileLabel->setToolTip(fileNameLocale);
        fileToFlash = file.fileName();
        file.close();
    }
}

void MainWindow::flashFW()
{
    if (checkProcRunning())
        return;

    QString msg = "";

    msg += tr("Make sure your AQ is connected via USB and is already in bootloader mode.  To enter bootloader mode,"
              "first connect the BOOT pins (or hold the BOOT button) and then turn the AQ on.\n\n");

    msg += "Do you wish to continue flashing?";

    QMessageBox::StandardButton qrply = QMessageBox::warning(this, tr("Confirm Firmware Flashing"), msg, QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Yes);
    if (qrply == QMessageBox::Cancel)
        return;

    activeProcessStatusWdgt = ui->textFlashOutput;
    fwFlashActive = true;

    flashFwDfu();

}

void MainWindow::flashFwDfu()
{
    QString AppPath = QDir::toNativeSeparators(aqBinFolderPath + "dfu-util" + platformExeExt);
    QStringList Arguments;
    Arguments.append("-a 0");                   // alt 0 is start of internal flash
    Arguments.append("-d 0483:df11" );          // device ident stm32
    Arguments.append("-s 0x08000000:leave");    // start address (:leave to exit DFU mode after flash)
    Arguments.append("-R");                     // reset after upload
    Arguments.append("-D");                     // firmware file
    Arguments.append(QDir::toNativeSeparators(ui->fileLabel->text()));

    QString cmdLine = AppPath;
    foreach (const QString arg, Arguments)
        cmdLine += " " + arg;
    ui->textFlashOutput->append(cmdLine + "\n\n");

    ps_master.start(AppPath , Arguments, QProcess::Unbuffered | QProcess::ReadWrite);
}

bool MainWindow::checkProcRunning(bool warn)
{
    if (ps_master.state() == QProcess::Running) {
        if (warn)
            showCriticalMessage(tr("Process already running."),tr("There appears to be an external process (calculation step or firmware flashing) already running. Please abort it first."));
        return true;
    }
    return false;
}

void MainWindow::prtstexit(int stat)
{
    prtstdout();
    if ( fwFlashActive ) {  // firmware flashing mode
        ui->flashButton->setEnabled(true);
        if (!stat)
            activeProcessStatusWdgt->insertPlainText("Successful. Restart the device.");
        fwFlashActive = false;
    }
}

void MainWindow::prtstdout()
{
    QString output = ps_master.readAllStandardOutput();
    if (output.contains(QRegExp("\\[(uWrote|H)"))) {
        output = output.replace(QRegExp(".\\[[uH]"), "");
        activeProcessStatusWdgt->clear();
    }
    activeProcessStatusWdgt->insertPlainText(output);
    activeProcessStatusWdgt->ensureCursorVisible();

    QStringList argument = ps_master.arguments();
    qDebug() << "Argument: " << argument;
}

QString MainWindow::extProcessError(QProcess::ProcessError err)
{
    QString msg;
    switch(err) {
    case QProcess::FailedToStart:
        msg = tr("Failed to start.");
        break;
    case QProcess::Crashed:
        msg = tr("Process terminated (aborted or crashed).");
        break;
    case QProcess::Timedout:
        msg = tr("Timeout waiting for process.");
        break;
    case QProcess::WriteError:
        msg = tr("Cannot write to process, exiting.");
        break;
    case QProcess::ReadError:
        msg = tr("Cannot read from process, exiting.");
        break;
    default:
        msg = tr("Unknown error");
        break;
    }
    return msg;
}

void MainWindow::showMessage(const QString &title, const QString &message, const QString &details, const QString severity)
{
    QMessageBox msgBox(this);
    msgBox.setWindowFlags(Qt::WindowStaysOnTopHint);
    msgBox.setParent(0);
    if (severity == "critical")
        msgBox.setIcon(QMessageBox::Critical);
    else if  (severity == "warning")
        msgBox.setIcon(QMessageBox::Warning);
    else
        msgBox.setIcon(QMessageBox::Information);
    msgBox.setText(title.leftJustified(300, ' '));
    msgBox.setInformativeText(message);
    if (!details.isEmpty())
        msgBox.setDetailedText(details);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.setDefaultButton(QMessageBox::Ok);
    msgBox.exec();
}

void MainWindow::showCriticalMessage(const QString &title, const QString &message)
{
    showMessage(title, message, "", "critical");
}

void MainWindow::heartbeatTimeout(bool timeout, unsigned int ms)
{
    if (timeout)
    {
        updateGUI(false);
        //        if (ms > 5000)
        //        {
        ////            ui->statusBar->showMessage(tr("Disconnected"));
        ////            ui->scrollArea->setStyleSheet(QString("background-color: red; border-radius: 12px"));
        ////            ui->btn_connectBoard->setText(QString("Connect to Board"));
        //            updateGUI(false);
        //            return;
        //        }
        //        else
        //        {
        //            if ((ms/100) % 2 == 0)
        //            {
        ////                ui->scrollArea->setStyleSheet(QString("background-color: yellow; border-radius: 12px"));
        //            }
        //            else
        //            {
        ////                ui->scrollArea->setStyleSheet(QString("background-color: red; border-radius: 12px"));
        //            }
        ////            ui->statusBar->showMessage(tr("Waiting..."));
        //        }
    }
    else
    {
        //        ui->scrollArea->setStyleSheet(QString("background-color: green; border-radius: 12px"));
        //        ui->statusBar->showMessage(tr("Connected"));
        updateGUI(true);
    }
}

void MainWindow::updateGUI(bool ready)
{
    if (ready)
    {
        ui->groupBox_SelectFW->setEnabled(true);
    }
    else
    {
        ui->groupBox_SelectFW->setEnabled(false);
    }
}

void MainWindow::up()
{
    updateGUI(false);
}


