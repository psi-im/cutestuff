/*
 * bytestream.h - base class for bytestreams
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

#ifndef CS_BYTESTREAM_H
#define CS_BYTESTREAM_H

#include<qobject.h>

class ByteStream : public QObject
{
	Q_OBJECT
public:
	ByteStream(QObject *parent=0);
	virtual ~ByteStream()=0;

	virtual bool isOpen() const;
	virtual void close();
	virtual int write(const QByteArray &)=0;
	virtual QByteArray read(int bytes=0)=0;
	virtual int bytesAvailable() const;
	virtual int bytesToWrite() const;

signals:
	void connectionClosed();
	void delayedCloseFinished();
	void readyRead();
	void bytesWritten(int);
	void error(int);
};

class QSSLFilter;

class SecureStream : public ByteStream
{
	Q_OBJECT
public:
	SecureStream(ByteStream *, QSSLFilter *, QObject *parent=0);
	~SecureStream();

	ByteStream *byteStream() const;

	void startHandshake();
	void waitForHandshake();

	bool isOpen() const;
	void close();
	int write(const QByteArray &)=0;
	QByteArray read(int bytes=0)=0;
	int bytesAvailable() const;
	int bytesToWrite() const;

signals:
	void handshaken();

private:
	class Private;
	Private *d;
};

#endif
