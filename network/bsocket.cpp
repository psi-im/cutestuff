/*
 * bsocket.cpp - QSocket wrapper based on Bytestream with SRV DNS support
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

#include"bsocket.h"

#include<qcstring.h>
#include<qsocket.h>
#include<qdns.h>
#include"ndns.h"
#include"srvresolver.h"

#ifdef BS_DEBUG
#include<stdio.h>
#endif

class BSocket::Private
{
public:
	Private() {}

	QSocket *qsock;
	int state;

	NDns ndns;
	SrvResolver srv;
	QString host;
	int port;
};

BSocket::BSocket(QObject *parent)
:ByteStream(parent)
{
	d = new Private;
	d->qsock = 0;
	connect(&d->ndns, SIGNAL(resultsReady()), SLOT(ndns_done()));
	connect(&d->srv, SIGNAL(resultsReady()), SLOT(srv_done()));

	reset();
}

BSocket::~BSocket()
{
	reset(true);
	delete d;
}

void BSocket::reset(bool clear)
{
	if(d->qsock) {
		d->qsock->deleteLater();
		d->qsock = 0;
	}
	if(d->srv.isBusy())
		d->srv.stop();
	if(d->ndns.isBusy())
		d->ndns.stop();
	if(clear)
		clearReadBuffer();
	d->state = Idle;
}

void BSocket::ensureSocket()
{
	if(!d->qsock) {
		d->qsock = new QSocket;
		connect(d->qsock, SIGNAL(connected()), SLOT(qs_connected()));
		connect(d->qsock, SIGNAL(connectionClosed()), SLOT(qs_connectionClosed()));
		connect(d->qsock, SIGNAL(delayedCloseFinished()), SLOT(qs_delayedCloseFinished()));
		connect(d->qsock, SIGNAL(readyRead()), SLOT(qs_readyRead()));
		connect(d->qsock, SIGNAL(bytesWritten(int)), SLOT(qs_bytesWritten(int)));
		connect(d->qsock, SIGNAL(error(int)), SLOT(qs_error(int)));
	}
}

void BSocket::connectToHost(const QString &host, Q_UINT16 port)
{
	if(d->qsock)
		d->qsock->close();
	reset(true);
	d->host = host;
	d->port = port;
	d->state = HostLookup;
	d->ndns.resolve(d->host);
}

void BSocket::connectToServer(const QString &srv, const QString &type)
{
	if(d->qsock)
		d->qsock->close();
	reset(true);
	d->state = HostLookup;
	d->srv.resolve(srv, type, "tcp");
}

void BSocket::setSocket(int s)
{
	if(d->qsock)
		d->qsock->close();
	reset(true);
	ensureSocket();
	d->qsock->setSocket(s);
	d->state = Connected;
}

int BSocket::state() const
{
	return d->state;
}

bool BSocket::isOpen() const
{
	if(d->state == Connected)
		return true;
	else
		return false;
}

void BSocket::close()
{
	if(d->state == Idle)
		return;

	if(d->qsock) {
		d->qsock->close();
		d->state = Closing;
		if(d->qsock->bytesToWrite() == 0)
			reset();
	}
	else {
		reset();
	}
}

int BSocket::write(const QByteArray &a)
{
	if(d->state != Connected)
		return -1;
#ifdef BS_DEBUG
	QCString cs;
	cs.resize(a.size()+1);
	memcpy(cs.data(), a.data(), a.size());
	QString s = QString::fromUtf8(cs);
	fprintf(stderr, "BSocket: writing [%d]: {%s}\n", a.size(), cs.data());
#endif
	return d->qsock->writeBlock(a.data(), a.size());
}

int BSocket::bytesToWrite() const
{
	if(!d->qsock)
		return 0;
	return d->qsock->bytesToWrite();
}

void BSocket::srv_done()
{
	if(d->srv.result()) {
		d->host = d->srv.resultString();
		d->port = d->srv.resultPort();
		do_connect();
	}
	else {
#ifdef BS_DEBUG
		fprintf(stderr, "BSocket: Error resolving hostname.\n");
#endif
		error(ErrHostNotFound);
	}
}

void BSocket::ndns_done()
{
	if(d->ndns.result()) {
		d->host = d->ndns.resultString();
		d->state = Connecting;
		do_connect();
	}
	else {
#ifdef BS_DEBUG
		fprintf(stderr, "BSocket: Error resolving hostname.\n");
#endif
		error(ErrHostNotFound);
	}
}

void BSocket::do_connect()
{
#ifdef BS_DEBUG
	fprintf(stderr, "BSocket: Connecting to %s:%d\n", d->host.latin1(), d->port);
#endif
	ensureSocket();
	d->qsock->connectToHost(d->host, d->port);
}

void BSocket::qs_connected()
{
	d->state = Connected;
#ifdef BS_DEBUG
	fprintf(stderr, "BSocket: Connected.\n");
#endif
	connected();
}

void BSocket::qs_connectionClosed()
{
#ifdef BS_DEBUG
	fprintf(stderr, "BSocket: Connection Closed.\n");
#endif
	reset();
	connectionClosed();
}

void BSocket::qs_delayedCloseFinished()
{
#ifdef BS_DEBUG
	fprintf(stderr, "BSocket: Delayed Close Finished.\n");
#endif
	reset();

	delayedCloseFinished();
}

void BSocket::qs_readyRead()
{
	// read in the block
	QByteArray block;
	int len = d->qsock->bytesAvailable();
	if(len < 1)
		len = 1024; // zero bytes available?  we'll assume a bogus value and default to 1024
	block.resize(len);
	int actual = d->qsock->readBlock(block.data(), len);
	if(actual < 1)
		return;
	block.resize(actual);

#ifdef BS_DEBUG
	QCString cs;
	cs.resize(block.size()+1);
	memcpy(cs.data(), block.data(), block.size());
	QString s = QString::fromUtf8(cs);
	fprintf(stderr, "BSocket: read [%d]: {%s}\n", block.size(), s.latin1());
#endif

	appendRead(block);
	readyRead();
}

void BSocket::qs_bytesWritten(int x)
{
#ifdef BS_DEBUG
	fprintf(stderr, "BSocket: BytesWritten [%d].\n", x);
#endif
	bytesWritten(x);
}

void BSocket::qs_error(int x)
{
#ifdef BS_DEBUG
	fprintf(stderr, "BSocket: Error.\n");
#endif
	// connection error during SRV host connect?  try next
	if(d->state == HostLookup && (x == QSocket::ErrConnectionRefused || x == QSocket::ErrHostNotFound)) {
		d->srv.next();
		return;
	}

	reset();
	if(x == QSocket::ErrConnectionRefused)
		error(ErrConnectionRefused);
	else if(x == QSocket::ErrHostNotFound)
		error(ErrHostNotFound);
	else if(x == QSocket::ErrSocketRead)
		error(ErrRead);
}
