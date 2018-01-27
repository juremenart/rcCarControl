#ifndef __HELPER_MACROS_H
#define __HELPER_MACROS_H

#include <QDebug>
#include <QMessageBox>

#include <stdexcept>


#define DEL_WIDGET(a) { if(a) { delete a; a = NULL; } }

#define CRITICAL_BOX(a) {                                               \
        qDebug() << a;                                                  \
        QMessageBox::critical(this, QString("Error"),                   \
                              a, QMessageBox::Close,                    \
                              QMessageBox::NoButton,                    \
                              QMessageBox::NoButton);                   \
        throw std::runtime_error((const char *)a.toLatin1().data()); }

#define WARNING_BOX(a) {                                                \
        qDebug() << a;                                                  \
        QMessageBox::warning(this, QString("Warning"),                  \
                             a, QMessageBox::Close,                     \
                             QMessageBox::NoButton,                     \
                             QMessageBox::NoButton); }

#endif //__HELPER_MACROS_H
