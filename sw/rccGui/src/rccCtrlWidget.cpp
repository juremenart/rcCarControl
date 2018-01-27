#include <QWidget>
#include <QDebug>
#include <QMessageBox>
#include <QCoreApplication>

#include "rccCtrlWidget.h"
#include "helperMacros.h"

rccCtrlWidget::rccCtrlWidget(QWidget *parent)
    : QWidget(parent), mTimer(NULL), mRcciClient(NULL), mDriveRegistered(false),
      mLogText(NULL), mLogReadThread(NULL)
{
    mMainLayout = new QGridLayout;
    setLayout(mMainLayout);

    mRccConnWidget   = new rccConnWidget(this);

    mLogText = new QPlainTextEdit();
    mLogText->setReadOnly(true);

    mMainLayout->addWidget(mRccConnWidget, 0, 0, 1, 4);
    mMainLayout->addWidget(mLogText, 1, 0, 5, 4);

    connect(mRccConnWidget, SIGNAL(rccConnect(QString)),
            this, SLOT(connRequest(QString)));
    connect(this, SIGNAL(addLogText(QString)),
            mLogText, SLOT(appendPlainText(QString)));

    mRcciClient = new rcciClient();
}

rccCtrlWidget::~rccCtrlWidget()
{
    emit driveRegistered(false);
    mDriveRegistered = false;
    if(mLogReadThread)
    {
        mLogReadThread->setRunningFalse();
        mLogReadThread->terminate();
        mLogReadThread->wait();
        DEL_WIDGET(mLogReadThread);
    }
    if(mRcciClient)
    {
        mRcciClient->drvDisconnect();
        mRcciClient->logDisconnect();
        mRcciClient->disconnect();
        DEL_WIDGET(mRcciClient);
    }
    DEL_WIDGET(mRccConnWidget);
    DEL_WIDGET(mTimer);
    DEL_WIDGET(mLogText);
}

void rccCtrlWidget::connRequest(QString uri)
{
    if(mRcciClient && mRcciClient->isConnected())
    {
        // TODO: Add this also to the destructor (separate file to cleanup)?
        // we assume we want to disconnect?
        mRcciClient->drvDisconnect();
        mRcciClient->logDisconnect();
        qDebug() << QString("Disconnecting from %1").arg(uri);

        mLogReadThread->setRunningFalse();
        mLogReadThread->terminate();
        mLogReadThread->wait();
        DEL_WIDGET(mLogReadThread);
        mRcciClient->disconnect();
        mRccConnWidget->connected(false);
        return;
    }
    QStringList uriList = uri.split(":");

    if(uriList.length() != 2)
    {
        WARNING_BOX(QString("Incorrect URI: \"%1\"\n"
                            "Expected: <hostname>:<port>").arg(uri));
        return;
    }

    QString hostname = uriList[0];
    QString port = uriList[1];

    qDebug() << QString("Hostname: %1 Port: %2").arg(hostname).arg(port);
    std::string logStr;
    if(mRcciClient->connect(hostname.toStdString(),
                            std::atoi(port.toStdString().c_str())) < 0)
    {
        WARNING_BOX(QString("Can not connect to %1").arg(uri));
        return;
    }

    if(mRcciClient->logConnect(logStr) < 0)
    {
        WARNING_BOX(QString("Can not register for logging on %1").arg(uri));
        return;
    }

    mLogText->clear();

    // start reading thread and push strings to text fields
    emit addLogText(QString::fromStdString(logStr));

    // start new thread class to read new log entries and connect them
    // to our log text widget
    if(!mLogReadThread)
    {
        mLogReadThread = new rccLogReadThread(this, mRcciClient);
        connect(mLogReadThread, SIGNAL(newLogText(const QString &)),
                mLogText, SLOT(appendPlainText(QString)));
        mLogReadThread->start();
    }

    qDebug() << QString("Connected to %1").arg(uri);
    mRccConnWidget->connected(true);
}

void rccCtrlWidget::driveRegister(bool aDrvReg)
{
    if(!mRcciClient)
    {
        mDriveRegistered = false;
        emit driveRegistered(false);
    }

    qDebug() << "rccCtrlWidget::driveRegister(): " << aDrvReg;

    if(aDrvReg)
    {
        if(mRcciClient->drvConnect() < 0)
        {
            WARNING_BOX(QString("Can not register drive service!"));
            mDriveRegistered = false;
            emit driveRegistered(false);
            return;
        }
        // connected
        mDriveRegistered = true;
        emit driveRegistered(true);
    }
    else
    {
        // !aDrvReg
        mRcciClient->drvDisconnect();
        mDriveRegistered = false;
        emit driveRegistered(false);
    }
}

void rccCtrlWidget::driveDataUpdate(int32_t drive, int32_t steer)
{
    if(mRcciClient && mDriveRegistered)
    {
        mRcciClient->drvSendData(drive, steer);
    }
}

