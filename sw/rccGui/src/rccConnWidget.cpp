#include <QLabel>
#include <QSpacerItem>
#include <QPixmap>
#include <QImage>

#include "helperMacros.h"
#include "rccConnWidget.h"

static const QString c_logoImage(":rc/logo.jpg");

rccConnWidget::rccConnWidget(QWidget *parent)
    : QWidget(parent), mMainLayout(NULL), mConnEdit(NULL), mConnBtn(NULL),
      mLogoLabel(NULL)
{
    mMainLayout = new QGridLayout();
    setLayout(mMainLayout);

    mMainLayout->addWidget(new QLabel(tr("Connect to: ")), 1, 1, 1, 1);
    mConnEdit = new QLineEdit(tr("192.168.0.12:1025"));

    mMainLayout->addWidget(mConnEdit, 1, 2, 1, 2);

    mConnBtn = new QPushButton();
    mConnBtn->setText(tr("Connect"));
    mMainLayout->addWidget(mConnBtn, 1, 4, 1, 3);

    mMainLayout->addItem(new QSpacerItem(1,1, QSizePolicy::Expanding,
                                         QSizePolicy::Fixed), 1, 7, 1, 3);

    mBitId = new QLabel();
    mMainLayout->addWidget(mBitId, 2, 1, 1, 7);

    connect(mConnBtn, SIGNAL(pressed()), this, SLOT(connPressed()));

    // TODO: Add some logo if you want :)
//    mLogoLabel = new QLabel();
//    QPixmap pm(c_logoImage);
//    mLogoLabel->setPixmap(pm.scaled(300, 100, Qt::KeepAspectRatio));
//    mMainLayout->addWidget(mLogoLabel, 1, 10, 2, 2);

    connected(false);
}

rccConnWidget::~rccConnWidget()
{
    DEL_WIDGET(mConnEdit);
    DEL_WIDGET(mConnBtn);
    DEL_WIDGET(mLogoLabel);
    DEL_WIDGET(mMainLayout);
}

void rccConnWidget::connPressed(void)
{
    QString uri(mConnEdit->text());

    emit rccConnect(uri);
}

void rccConnWidget::connected(bool a_connected)
{
    if(a_connected)
    {
        mConnEdit->setEnabled(false);
        mConnBtn->setText(tr("Disconnect"));
    }
    else
    {
        mConnEdit->setEnabled(true);
        mConnBtn->setText(tr("Connect"));
    }
}
