#include <QTabWidget>
#include <QToolButton>
#include <QLabel>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "helperMacros.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent), ui(new Ui::MainWindow),
    mMainWidget(NULL), mMainLayout(NULL), mTabWidget(NULL),
    mCtrlWidget(NULL), mDrvWidget(NULL)
{
    ui->setupUi(this);

    setWindowTitle(tr("RC Car Control GUI"));
    mMainLayout = new QGridLayout;
    mTabWidget  = new QTabWidget(this);

    mCtrlWidget = new rccCtrlWidget(this);
    mDrvWidget  = new rccDrvWidget(this);

    // Connect some signals/slots for communication
    // mDrvWidget->mCtrlWidget
    connect(mDrvWidget, SIGNAL(driveRegister(bool)),
            mCtrlWidget, SLOT(driveRegister(bool)));
    connect(mDrvWidget, SIGNAL(driveDataUpdated(int32_t, int32_t)),
            mCtrlWidget, SLOT(driveDataUpdate(int32_t, int32_t)));
    // mCtrlWidget->mDrvWidget
    connect(mCtrlWidget, SIGNAL(driveRegistered(bool)),
            mDrvWidget, SLOT(driveRegistered(bool)));

    mTabWidget->addTab(mCtrlWidget, tr("Ctrl"));
    mTabWidget->addTab(mDrvWidget, tr("Drv"));

    mMainLayout->addWidget(mTabWidget);

    mMainWidget = new QWidget(this);
    mMainWidget->setLayout(mMainLayout);
    setCentralWidget(mMainWidget);
}

MainWindow::~MainWindow()
{
//    DEL_WIDGET(mDrvWidget);
    DEL_WIDGET(mCtrlWidget);

    delete ui;
}
