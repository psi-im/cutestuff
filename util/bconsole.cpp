/*
 * bconsole.cpp - ByteStream wrapper for stdin/stdout
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

#include"bconsole.h"

#include<qsocketnotifier.h>
#include<unistd.h>

//----------------------------------------------------------------------------
// BConsole
//----------------------------------------------------------------------------
class BConsole::Private
{
public:
	Private() {}

	QSocketNotifier *sn;
	QByteArray sendBuf, recvBuf;
};

BConsole::BConsole(QObject *parent)
:ByteStream(parent)
{
	d = new Private;
	d->sn = new QSocketNotifier(0, QSocketNotifier::Read);
	connect(d->sn, SIGNAL(activated(int)), SLOT(sn_dataReady()));
}

BConsole::~BConsole()
{
	delete d->sn;
	delete d;
}

bool BConsole::isOpen() const
{
	return false;
}

void BConsole::close()
{
}

int BConsole::write(const QByteArray &a)
{
	int r = ::write(1, a.data(), a.size());
	return r;
}

QByteArray BConsole::read(int bytes)
{
	QByteArray a;
	if(bytes == 0) {
		a = d->recvBuf.copy();
		d->recvBuf.resize(0);
	}
	else {
		a.resize(bytes);
		char *r = d->recvBuf.data();
		int newsize = d->recvBuf.size()-bytes;
		memcpy(a.data(), r, bytes);
		memmove(r, r+bytes, newsize);
		d->recvBuf.resize(newsize);
	}
	return a;
}

int BConsole::bytesAvailable() const
{
	return d->recvBuf.size();
}

int BConsole::bytesToWrite() const
{
	return 0;
}

void BConsole::sn_dataReady()
{
	QByteArray a(1024);
	int r = ::read(0, a.data(), a.size());
	if(r < 0) {
		error(ErrRead);
	}
	else if(r == 0) {
		connectionClosed();
	}
	else {
		a.resize(r);
		int oldsize = d->recvBuf.size();
		d->recvBuf.resize(oldsize + a.size());
		memcpy(d->recvBuf.data() + oldsize, a.data(), a.size());

		readyRead();
	}
}
