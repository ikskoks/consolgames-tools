include(../../ResidentEvil.pri)

CONFIG += qtestlib
CONFIG -= flat
TEMPLATE = app

HEADERS = $$files(*.h)
SOURCES = $$files(*.cpp)

LIBS += Compression.lib core.lib