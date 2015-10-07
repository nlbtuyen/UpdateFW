message(Qt version $$[QT_VERSION])

TARGET = UpdateFW
TEMPLATE = app

win* {
        message(Windows build)
        CONFIG += WinBuild
}

# debug/release
CONFIG(debug, debug|release) {
        message(Debug build)
        CONFIG += DebugBuild
} else:CONFIG(release, debug|release) {
        message(Release build)
        CONFIG += ReleaseBuild
} else {
        error(Unsupported build type)
}

# Qt configuration
QT += widgets webkitwidgets concurrent quickwidgets quick gui

# build directories
DESTDIR = $${OUT_PWD}
BUILDDIR = $${OUT_PWD}/build
OBJECTS_DIR = $${BUILDDIR}/obj
MOC_DIR = $${BUILDDIR}/moc
UI_DIR = $${BUILDDIR}/ui
RCC_DIR = $${BUILDDIR}/rcc
MOC_DIR = $${BUILDDIR}/moc
# root of project files
BASEDIR = $$_PRO_FILE_PWD_

MAVLINK_CONF = "autoquad"
MAVLINKPATH = $$BASEDIR/mavlink

# OS-specific external libs and settings
WinBuild: include(config_Windows.pri)

# common external libs
include(config_external_libs.pri)

# post-build steps
include(install.pri)

# Input
include(src/src.pri)


RESOURCES += \
    terminal.qrc

DEFINES *= QT_USE_QSTRINGBUILDER

