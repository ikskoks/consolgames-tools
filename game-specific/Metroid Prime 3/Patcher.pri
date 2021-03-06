CONFIG(debug, debug|release) {
	CONFIG_NAME = debug
} else {
	CONFIG_NAME = release
}

DEFINES += _SCL_SECURE_NO_WARNINGS _CRT_SECURE_NO_WARNINGS

INCLUDEPATH += \
	$$PWD/ExtractorLib \
	$$PWD/PasterLib \
	$$PWD/externals/WiiStreams \
	$$PWD/externals/WiiStreams/include \
	$$PWD/externals/WiiStreams/include/openssl \
	$$PWD/externals/core \
	$$PWD/externals/core/streams \
	$$PWD/externals/core/classes \

QMAKE_LIBDIR += \
	$$PWD/ExtractorLib/$$CONFIG_NAME \
	$$PWD/PasterLib/$$CONFIG_NAME \
	$$PWD/externals/core/$$CONFIG_NAME \
	$$PWD/externals/WiiStreams/$$CONFIG_NAME \
