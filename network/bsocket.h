/*
 * bsocket.h - QSocket wrapper based on Bytestream with SRV DNS support
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

#ifndef CS_BSOCKET_H
#define CS_BSOCKET_H

#include<qobject.h>
#include"../util/bytestream.h"

class BSocket : public ByteStream
{
	Q_OBJECT
public:
	enum Error { ErrConnectionRefused = ErrCustom, ErrHostNotFound };
	enum State { Idle, HostLookup, Connecting, Connected, Closing };
	BSocket(QObject *parent=0);
	~BSocket();

	void connectToHost(const QString &host, Q_UINT16 port);
	void connectToServer(const QString &srv, const QString &type);
	void setSocket(int);
	int state() const;

	// from ByteStream
	bool isOpen() const;
	void close();
	int write(const QByteArray &);
	int bytesToWrite() const;

signals:
	void connected();

private slots:
	void qs_connected();
	void qs_connectionClosed();
	void qs_delayedCloseFinished();
	void qs_readyRead();
	void qs_bytesWritten(int);
	void qs_error(int);
	void qdns_done();
	void ndns_done();

private:
	class Private;
	Private *d;

	void reset(bool clear=false);
	void do_connect();
	void ensureSocket();
};

#endif
