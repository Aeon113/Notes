TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    virmouse.c \
    virmouse2.c

DISTFILES += \
    Makefile

DEFINES += __KERNEL__
DEFINES += MODULE
INCLUDEPATH = /lib/modules/4.15.0-32-generic/build \
/lib/modules/4.15.0-32-generic/build/include \
/lib/modules/4.15.0-32-generic/build/arch/x86 \
/lib/modules/4.15.0-32-generic/build/arch/x86/include

HEADERS += \
    mymouse.h
