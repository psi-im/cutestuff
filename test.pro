CONFIG += thread
TARGET  = test

HEADERS = \
	util/bytestream.h \
	network/ndns.h \
	network/bsocket.h \
	network/httpconnect.h

SOURCES = \
	util/bytestream.cpp \
	network/ndns.cpp \
	network/bsocket.cpp \
	network/httpconnect.cpp \
	test.cpp

