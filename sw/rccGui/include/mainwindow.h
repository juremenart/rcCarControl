#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGridLayout>
#include <QWidget>
#include <QString>
#include <QTabWidget>

#include "rccCtrlWidget.h"
#include "rccDrvWidget.h"
#include "rccCamWidget.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

public slots:

private:
    Ui::MainWindow *ui;

    QWidget        *mMainWidget;
    QGridLayout    *mMainLayout;
    QTabWidget     *mTabWidget;

    rccCtrlWidget  *mCtrlWidget;
    rccDrvWidget   *mDrvWidget;
    rccCamWidget   *mCamWidget;
};

#endif // MAINWINDOW_H
