/*
 * bytestream.cpp - base class for bytestreams
 * Copyright (C) 2003  Justin Karneges
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include"bytestream.h"

//----------------------------------------------------------------------------
// ByteStream
//----------------------------------------------------------------------------
class ByteStream::Private
{
public:
	Private() {}

	QByteArray readBuf, writeBuf;
};

ByteStream::ByteStream(QObject *parent)
:QObject(parent)
{
	d = new Private;
}

ByteStream::~ByteStream()
{
	delete d;
}

bool ByteStream::isOpen() const
{
	return false;
}

void ByteStream::close()
{
}

int ByteStream::write(const QByteArray &a)
{
	if(!isOpen())
		return -1;

	bool doWrite = bytesToWrite() == 0 ? true: false;
	appendWrite(a);
	if(doWrite)
		return tryWrite();
	else
		return 0;
}

QByteArray ByteStream::read(int bytes)
{
	return takeRead(bytes);
}

int ByteStream::bytesAvailable() const
{
	return d->readBuf.size();
}

int ByteStream::bytesToWrite() const
{
	return d->writeBuf.size();
}

int ByteStream::write(const QCString &cs)
{
	QByteArray block(cs.length());
	memcpy(block.data(), cs.data(), block.size());
	return write(block);
}

void ByteStream::clearReadBuffer()
{
	d->readBuf.resize(0);
}

void ByteStream::clearWriteBuffer()
{
	d->writeBuf.resize(0);
}

void ByteStream::appendRead(const QByteArray &block)
{
	appendArray(&d->readBuf, block);
}

void ByteStream::appendWrite(const QByteArray &block)
{
	appendArray(&d->writeBuf, block);
}

QByteArray ByteStream::takeRead(int size, bool del)
{
	return takeArray(&d->readBuf, size, del);
}

QByteArray ByteStream::takeWrite(int size, bool del)
{
	return takeArray(&d->writeBuf, size, del);
}

QByteArray & ByteStream::readBuf()
{
	return d->readBuf;
}

QByteArray & ByteStream::writeBuf()
{
	return d->writeBuf;
}

int ByteStream::tryWrite()
{
	return -1;
}

void ByteStream::appendArray(QByteArray *a, const QByteArray &b)
{
	int oldsize = a->size();
	a->resize(oldsize + b.size());
	memcpy(a->data() + oldsize, b.data(), b.size());
}

QByteArray ByteStream::takeArray(QByteArray *from, int size, bool del)
{
	QByteArray a;
	if(size == 0) {
		a = from->copy();
		if(del)
			from->resize(0);
	}
	else {
		if(size > (int)from->size())
			size = from->size();
		a.resize(size);
		char *r = from->data();
		memcpy(a.data(), r, size);
		if(del) {
			int newsize = from->size()-size;
			memmove(r, r+size, newsize);
			from->resize(newsize);
		}
	}
	return a;
}
