#ifndef __RCC_CTRL_WIDGET_H
#define __RCC_CTRL_WIDGET_H

#include <QWidget>
#include <QString>
#include <QGridLayout>
#include <QTimer>
#include <QGroupBox>
#include <QPlainTextEdit>
#include <QSensor>
#include <QSensorReading>

#include "rcci_client.h"
#include "rccConnWidget.h"
#include "rccLogReadThread.h"

QT_USE_NAMESPACE

class rccCtrlWidget : public QWidget
{
    Q_OBJECT

public:
    rccCtrlWidget(QWidget *parent = 0);
    ~rccCtrlWidget();

signals:
    void addLogText(const QString &a_text);
    void driveRegistered(bool aDrvReg);

public slots:
    void connRequest(QString uri);
    void driveRegister(bool aDrvReg);
    void driveDataUpdate(int32_t drive, int32_t steer);

//    void timerTimeout(void);

private:
    QGridLayout      *mMainLayout;
    QTimer           *mTimer;

    rccConnWidget    *mRccConnWidget;

    rcciClient       *mRcciClient;
    bool              mDriveRegistered;

    QPlainTextEdit   *mLogText;
    rccLogReadThread *mLogReadThread;
};

#endif // __RCC_CTRL_WIDGET_H
