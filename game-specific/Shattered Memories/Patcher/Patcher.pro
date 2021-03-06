include(Patcher.pri)

QT += core gui
CONFIG -= flat

!lessThan(QT_VER_MAJ, 5) {
	QT += widgets
}

HEADERS = \
	$$files(*.h) \
	$$files(wizard/*.h) \

SOURCES = \
	$$files(*.cpp) \
	$$files(wizard/*.cpp) \

FORMS = \
	$$files(ui/*.ui) \
	$$files(wizard/ui/*.ui) \

RESOURCES = \
	resources/Patcher.qrc \
	patchdata.qrc \

isEmpty(NO_PRECOMPILED_HEADER){
	CONFIG += precompile_header
	PRECOMPILED_HEADER = pch.h
	SOURCES -= pch.h.cpp
}

RC_FILE = Patcher.rc
 
TEMPLATE = app
QMAKE_LFLAGS += /OPT:REF

CONFIG(debug, debug|release){
	CONFIG += console
}

LIBS += \
	core.lib \
	zlib.lib \
	WiiStreams.lib \
	Common.lib \
	CommonQt.lib \
	Compression.lib \
	StreamParserLib.lib \
	PatcherLib.lib \

MOC_DIR = generatedfiles/moc
UI_HEADERS_DIR = generatedfiles/ui
UI_SOURCES_DIR = generatedfiles/ui
RCC_DIR = generatedfiles/rcc

INCLUDEPATH += \
	$$PWD \
	$$PWD/wizard \
	$$PWD/$$UI_HEADERS_DIR \
