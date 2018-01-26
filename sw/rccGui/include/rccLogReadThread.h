#ifndef __RCC_LOG_READ_WIDGET_H
#define __RCC_LOG_READ_WIDGET_H

#include <QWidget>
#include <QThread>
#include <QDebug>

#include "rcci_client.h"

QT_USE_NAMESPACE

class rccLogReadThread : public QThread
{
    Q_OBJECT

public:
    rccLogReadThread(QWidget *parent, rcciClient *aRcciClient)
        : QThread(parent)
    {
        mRcciClient = aRcciClient;
    }

    void setRunningFalse() { mRunning = false; };

signals:
    void newLogText(const QString &a_text);

private:
    // TODO: Protect mRunning with mutexes
    void run() override
    {
        if(!mRcciClient)
            return;

        mRunning = true;
        while(mRunning)
        {
            std::string logStr;
            if(mRcciClient->logReadData(logStr) > 0)
            {
                // TODO: Remove the stupid new-line after each message for QPlainTextEdit
                emit newLogText(QString::fromStdString(logStr));
            }
        }
    };


private:
    bool        mRunning;
    rcciClient *mRcciClient;
};

#endif // __RCC_LOG_READ_WIDGET_H
