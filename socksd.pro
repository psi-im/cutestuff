CONFIG += thread
TARGET  = socksd

DEFINES += PROX_DEBUG

HEADERS = \
	util/bytestream.h \
	util/bconsole.h \
	network/ndns.h \
	network/bsocket.h \
	network/socks.h \
	socksd.h

SOURCES = \
	util/bytestream.cpp \
	util/bconsole.cpp \
	network/ndns.cpp \
	network/bsocket.cpp \
	network/socks.cpp \
	socksd.cpp

