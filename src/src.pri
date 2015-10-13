
INCLUDEPATH += $$PWD \


SOURCES +=  $$PWD/main.cpp\
            $$PWD/mainwindow.cpp \
            $$PWD/uas.cpp \
            $$PWD/mavlinkprotocol.cpp \
            $$PWD/qgc.cpp \
            $$PWD/uasmanager.cpp \
            $$PWD/linkmanager.cpp \
            $$PWD/mavlinkuasfactory.cpp \
            $$PWD/seriallink.cpp \
            $$PWD/mavlinkdecoder.cpp \
            $$PWD/commconfigurationwindow.cpp \
            $$PWD/serialconfigurationwindow.cpp \
            $$PWD/uasparammanager.cpp

HEADERS  += $$PWD/mainwindow.h \
            $$PWD/uasinterface.h \
            $$PWD/uas.h \
            $$PWD/mavlinkprotocol.h \
            $$PWD/protocolinterface.h \
            $$PWD/autoquadmav.h \
            $$PWD/qgc.h \
            $$PWD/uasmanager.h \
            $$PWD/linkinterface.h \
            $$PWD/linkmanager.h \
            $$PWD/mavlinkuasfactory.h \
            $$PWD/seriallink.h \
            $$PWD/seriallinkinterface.h \
            $$PWD/mavlinkdecoder.h \
            $$PWD/mg.h \
            $$PWD/commconfigurationwindow.h \
            $$PWD/serialconfigurationwindow.h \
            $$PWD/uasparammanager.h

FORMS    += $$PWD/mainwindow.ui \
            $$PWD/commsettings.ui \
            $$PWD/serialsettings.ui

