#ifndef __RCC_CONN_WIDGET_H
#define __RCC_CONN_WIDGET_H

#include <QWidget>
#include <QGridLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

QT_USE_NAMESPACE

class rccConnWidget : public QWidget
{
    Q_OBJECT

public:
    rccConnWidget(QWidget *parent = 0);
    ~rccConnWidget();

    void connected(bool a_connected);

signals:
    void rccConnect(QString a_uri);

public slots:
    void connPressed(void);

private:
    QGridLayout *mMainLayout;
    QLineEdit   *mConnEdit;
    QPushButton *mConnBtn;
    QLabel      *mBitId;
    QLabel      *mLogoLabel;
};

#endif // __RCC_CONN_WIDGET_H
