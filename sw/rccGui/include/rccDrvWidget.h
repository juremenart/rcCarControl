#ifndef __RCC_DRV_WIDGET_H
#define __RCC_DRV_WIDGET_H

#include <QWidget>
#include <QString>
#include <QGridLayout>
#include <QTimer>
#include <QGroupBox>
#include <QPlainTextEdit>
#include <QSensor>
#include <QSensorReading>
#include <QCheckBox>

QT_USE_NAMESPACE

class rccDrvWidget : public QWidget
{
    Q_OBJECT

public:
    rccDrvWidget(QWidget *parent = 0);
    ~rccDrvWidget();

signals:
    void driveRegister(bool aDrvReg);
    void driveDataUpdated(int32_t drive, int32_t steer);

public slots:
    void driveRegistered(bool aDrvReg);
    void driveReadingChanged(void);

    // GUI stuff slots
    void cbDriveEnableChanged(int state);
    void gbSensorEnableChanged(bool on);

private:
    // GUI things
    QGridLayout      *mMainLayout;
    QCheckBox        *mCbDriveEnable;
    QGroupBox        *mGbSensorGroup;

    bool              mDriveRegistered;

    QSensor          *mDriveSensor;
    uint8_t           mDriveCount;
};

#endif // __RCC_DRV_WIDGET_H
