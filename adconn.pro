CONFIG += thread
TARGET  = adconn

DEFINES += PROX_DEBUG

HEADERS = \
	util/bytestream.h \
	util/bconsole.h \
	network/ndns.h \
	network/bsocket.h \
	network/httpconnect.h \
	adconn.h

SOURCES = \
	util/bytestream.cpp \
	util/bconsole.cpp \
	network/ndns.cpp \
	network/bsocket.cpp \
	network/httpconnect.cpp \
	adconn.cpp

