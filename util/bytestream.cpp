/*
 * bytestream.cpp - base class for bytestreams
 * Copyright (C) 2001, 2002  Justin Karneges
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

#include"../ssl/qssl.h"

//----------------------------------------------------------------------------
// ByteStream
//----------------------------------------------------------------------------
ByteStream::ByteStream(QObject *parent)
:QObject(parent)
{
}

ByteStream::~ByteStream()
{
}

bool ByteStream::isOpen() const
{
	return false;
}

void ByteStream::close()
{
}

int ByteStream::bytesAvailable() const
{
	return 0;
}

int ByteStream::bytesToWrite() const
{
	return 0;
}


//----------------------------------------------------------------------------
// SecureStream
//----------------------------------------------------------------------------
class SecureStream::Private
{
public:
	Private() {}

	QSSLFilter *ssl;
	ByteStream *bs;
};

SecureStream::SecureStream(ByteStream *bs, QSSLFilter *ssl, QObject *parent)
:ByteStream(parent)
{
	d = new Private;
	d->bs = bs;
	d->ssl = ssl;
}

SecureStream::~SecureStream()
{
	delete d->ssl;
	delete d->bs;
	delete d;
}

ByteStream *SecureStream::byteStream() const
{
	return d->bs;
}

void SecureStream::startHandshake()
{
}

void SecureStream::waitForHandshake()
{
}

bool SecureStream::isOpen() const
{
	return false;
}

void SecureStream::close()
{
}

int SecureStream::write(const QByteArray &)
{
	return 0;
}

QByteArray SecureStream::read(int bytes)
{
	return QByteArray();
}

int SecureStream::bytesAvailable() const
{
	return 0;
}

int SecureStream::bytesToWrite() const
{
	return 0;
}
