CONFIG += thread
TARGET  = test

HEADERS = \
	securesocket/ndns.h \
	securesocket/securesocket.h \
	util/bytestream.h

SOURCES = \
	test.cpp \
	securesocket/ndns.cpp \
	securesocket/securesocket.cpp \
	util/bytestream.cpp

