CONFIG += thread
TARGET  = enctest

INCLUDEPATH += /usr/local

HEADERS = \
	util/bytestream.h \
	util/base64.h \
	util/cipher.h \
	util/sha1.h \
	xmlsec/xmlenc.h

SOURCES = \
	util/bytestream.cpp \
	util/base64.cpp \
	util/cipher.cpp \
	util/sha1.cpp \
	xmlsec/xmlenc.cpp \
	enctest.cpp

LIBS += -L/usr/local/lib -lcrypto

