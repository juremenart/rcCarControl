#-------------------------------------------------
#
# Qt project file for rcCarControl
#
#-------------------------------------------------

QT       += core gui sensors

# Enable to use QWT for graphs - not used for now (from old projects)
# CONFIG   += qwt

# Enable debugging - remove for any 'serious' build :-)
#CONFIG += debug console

#QMAKE_CXXFLAGS += -g

# Enable this for some C++ defines
#QMAKE_CXXFLAGS += -DCOMM_SYNIO

# OS dependent stuff
linux:!android {
# For now USB support only for GNU/Linux
   DESTDIR = obj/rccGui/linux/

   # Qwt stuff
#   QWT_INSTALLDIR=/usr/local/qwt-6.1.4-svn/
#   QMAKE_INCDIR += $${QWT_INSTALLDIR}/include
#   LIBS += -L$${QWT_INSTALLDIR}/lib -lqwt     
}

win32 {
   # WinSock2 for Windows Sockets
   LIBS   += -lws2_32

   DESTDIR = obj/rccGui/win32

   # Qwt stuff
#   QWT_INSTALLDIR=C:/Qwt-6.1.3/
#   QMAKE_INCDIR += $${QWT_INSTALLDIR}/include
#   LIBS += -L$${QWT_INSTALLDIR}/lib -lqwt     
}

macx {
   DESTDIR = obj/rccGui/macos/

   # Qwt
#   include ( /Users/jure/Qt/qwt-6.1.4-svn/features/qwt.prf ) 
}

android {
# Building instructions (from my Mac, Qt installed, Qwt compiled and installed)
# ~/Qt/5.6/5.6/android_armv7/bin/qmake lgui.pro
# make
# make install INSTALL_ROOT=/Users/jure/work/llrf4/llrf4_src/runtime/lgui/android
# ~/Qt/5.6/5.6/android_armv7/bin/androiddeployqt --output ./android --input android/android-liblgui.so-deployment-settings.json --ant /Users/jure/Android/apache-ant-1.9.7/bin/ant
 #  CONFIG += qwt
   DESTDIR = obj/rccGui/android

#   QWT_INSTALLDIR=/Users/jure/Qt/qwt-android-6.1.4-svn/
#   LIBS += -L$${QWT_INSTALLDIR}/libs/armeabi-v7a/ -lqwt
#   QMAKE_INCDIR += $${QWT_INSTALLDIR}/include
#   ANDROID_EXTRA_LIBS += $${QWT_INSTALLDIR}/libs/armeabi-v7a/libqwt.so
}

   ios {
# Building (version is important, olders dont work)
#   ~/Qt/5.6.1/5.6/ios/bin/qmake lgui.pro
#  XCode project is created 'lgui.xcodeproj' (needs signing & identity check)
#  (Be careful 'bundle identifier' is wrong, should be changed to 'lgui' only
#   CONFIG += qwt
   DESTDIR = obj/rccGui/ios

#   QWT_INSTALLDIR=/Users/jure/Qt/qwt-ios-6.1.4-svn/
#   LIBS += -L$${QWT_INSTALLDIR}/lib -lqwt
#   QMAKE_INCDIR += $${QWT_INSTALLDIR}/include

#   QMAKE_XCODE_PROVISIONING_PROFILE = ba5850ec-b49f-46be-be09-453e154f9069
#   MY_DEVELOPMENT_TEAM.name = "Kristina Mitrovic Menart"
#   MY_DEVELOPMENT_TEAM.value = XXXXXX99XX
#   QMAKE_MAC_XCODE_SETTINGS += "Kristina Mitrovic Menart"
}
   
TARGET = rccGui
TEMPLATE = app

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

QMAKE_INCDIR+=include/ ../interface

MOC_DIR = $${DESTDIR}/moc/
UI_DIR = $${DESTDIR}/ui/
OBJECTS_DIR = $${DESTDIR}/obj/

RESOURCES = rccGui.qrc

# Application
SOURCES += src/main.cpp src/mainwindow.cpp src/rccCtrlWidget.cpp src/rccConnWidget.cpp ../interface/rcci_client.cpp src/rccDrvWidget.cpp

HEADERS += include/mainwindow.h include/rccConnWidget.h include/rccCtrlWidget.h ../interface/rcci_client.h ../interface/rcci_type.h include/rccLogReadThread.h include/rccDrvWidget.h

FORMS   += ui/mainwindow.ui

#qwt {
#  SOURCES += src/llrfRawQwt.cpp
#  HEADERS += include/llrfRawQwt.h
#
#  QMAKE_CXXFLAGS += -DGRAPH_QWT
#}
