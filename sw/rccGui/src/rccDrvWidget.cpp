#include <QWidget>
#include <QDebug>
#include <QMessageBox>
#include <QCoreApplication>

#include "rccDrvWidget.h"
#include "helperMacros.h"
#include "rcci_type.h"

rccDrvWidget::rccDrvWidget(QWidget *parent)
    : QWidget(parent), mDriveRegistered(false), mDriveSensor(NULL)
{
    mMainLayout = new QGridLayout;
    setLayout(mMainLayout);

    // instantiate accelerometer and check if exists
    mDriveSensor = new QSensor("QAccelerometer");
    // TODO: Make update rate programable
    mDriveSensor->setDataRate(10);
    connect(mDriveSensor, SIGNAL(readingChanged(void)),
            this, SLOT(driveReadingChanged(void)));

    mCbDriveEnable = new QCheckBox(this);
    mCbDriveEnable->setText(tr("Enable drive mode"));
    connect(mCbDriveEnable, SIGNAL(stateChanged(int)),
            this, SLOT(cbDriveEnableChanged(int)));

    mGbSensorGroup = new QGroupBox(this);
//    connect(mGbSensorGroup, SIGNAL(toggled(bool)),
//            this, SLOT(gbSensorEnableChanged(bool)));

    if(mDriveSensor && mDriveSensor->connectToBackend())
    {
        mGbSensorGroup->setTitle(tr("Use motion sensor"));
    }
    else
    {
        mGbSensorGroup->setTitle(tr("Use motion sensor (not supported on this "
                                    " platform)"));
    }
    mGbSensorGroup->setCheckable(true);
    mGbSensorGroup->setChecked(false);
    mGbSensorGroup->setEnabled(false);

    mMainLayout->addWidget(mCbDriveEnable);
    mMainLayout->addWidget(mGbSensorGroup);

    // this only requests drive control - the controller responds
    // with another signal that shows if it's indeed registered or
    // not
    emit driveRegister(true);
}

rccDrvWidget::~rccDrvWidget()
{
    emit driveRegister(false);
    mDriveSensor->stop();
    mDriveRegistered = false;
    DEL_WIDGET(mDriveSensor);

    DEL_WIDGET(mGbSensorGroup);
    DEL_WIDGET(mCbDriveEnable);
}

void rccDrvWidget::driveReadingChanged(void)
{
    if(mDriveSensor)
    {
        const qreal maxValue = 10.0; // TODO: Check, make it programable?
        qreal x = mDriveSensor->reading()->value(0).value<qreal>();
        qreal y = mDriveSensor->reading()->value(1).value<qreal>();
        qreal z = mDriveSensor->reading()->value(2).value<qreal>();

        qDebug() << "Sensor reading x=" << x << " y=" << y << " z=" << z;
        // If horizontal we take 'y' and 'z' and normalize it to int32
        int32_t drive = static_cast<int32_t>((z * rcci_msg_drv_max_param) / maxValue);
        int32_t steer = static_cast<int32_t>((y * rcci_msg_drv_max_param) / maxValue);
        emit driveDataUpdated(drive, steer);
    }
}

void rccDrvWidget::driveRegistered(bool aDrvReg)
{
    // this is caused by emiting driveRegister() signal (see
    // cbDriveEnableChaged())
    qDebug() << "rccDrvWidget::driveRegistered(): " << aDrvReg;
    mDriveRegistered = aDrvReg;
    if(aDrvReg)
    {
        // enable all possible buttons/boxes/..
        mCbDriveEnable->setChecked(Qt::Checked);
        if(mDriveSensor && mDriveSensor->connectToBackend())
        {
            mGbSensorGroup->setEnabled(true);
        }
    }
    else
    {
        mCbDriveEnable->setCheckState(Qt::Unchecked);
        mGbSensorGroup->setEnabled(false);
    }
}

void rccDrvWidget::cbDriveEnableChanged(int state)
{
    if(state == Qt::Checked)
    {
        // request drive service register
        emit driveRegister(true);
    }
    else
    {
        emit driveRegister(false);
    }
}

void rccDrvWidget::gbSensorEnableChanged(bool on)
{
    if(!mDriveSensor)
        return;

    qDebug() << "rccDrvWidget::gbSensorEnableChanged(): " << on;

    if(on == true)
    {
        mDriveSensor->start();
    }
    else
    {
        mDriveSensor->stop();
    }
}
