CONFIG += thread
TARGET  = adconn

DEFINES += BS_DEBUG PROX_DEBUG

HEADERS = \
	util/bytestream.h \
	util/base64.h \
	util/sha1.h \
	util/bconsole.h \
	network/ndns.h \
	network/bsocket.h \
	network/httpconnect.h \
	network/httppoll.h \
	network/socksclient.h \
	sasl/qsasl.h \
	adconn.h

SOURCES = \
	util/bytestream.cpp \
	util/base64.cpp \
	util/sha1.cpp \
	util/bconsole.cpp \
	network/ndns.cpp \
	network/bsocket.cpp \
	network/httpconnect.cpp \
	network/httppoll.cpp \
	network/socksclient.cpp \
	sasl/qsasl.cpp \
	adconn.cpp

LIBS += -lsasl2

