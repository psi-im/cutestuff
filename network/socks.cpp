/*
 * socks.cpp - SOCKS5 TCP proxy client/server
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

#include"socks.h"

#include<qhostaddress.h>
#include<qstringlist.h>
#include<qptrlist.h>
#include<qtimer.h>
#include<netinet/in.h>
#include"servsock.h"
#include"bsocket.h"

#ifdef PROX_DEBUG
#include<stdio.h>
#endif

#ifdef CS_NAMESPACE
namespace CS_NAMESPACE {
#endif

//----------------------------------------------------------------------------
// SocksClient
//----------------------------------------------------------------------------

// spc = socks packet client
// sps = socks packet server
// SPCS = socks packet client struct
// SPSS = socks packet server struct

// Version
static QByteArray spc_set_version()
{
	QByteArray ver(4);
	ver[0] = 0x05; // socks version 5
	ver[1] = 0x02; // number of methods
	ver[2] = 0x00; // no-auth
	ver[3] = 0x02; // username
	return ver;
}

static QByteArray sps_set_version(int method)
{
	QByteArray ver(2);
	ver[0] = 0x05;
	ver[1] = method;
	return ver;
}

struct SPCS_VERSION
{
	unsigned char version;
	QByteArray methodList;
};

static int spc_get_version(QByteArray *from, SPCS_VERSION *s)
{
	if(from->size() < 2)
		return 0;
	uint num = from->at(1);
	if(from->size() < 2 + num)
		return 0;
	QByteArray a = ByteStream::takeArray(from, 2+num);
	s->version = a[0];
	s->methodList.resize(num);
	memcpy(s->methodList.data(), a.data() + 2, num);
	return 1;
}

struct SPSS_VERSION
{
	unsigned char version;
	unsigned char method;
};

static int sps_get_version(QByteArray *from, SPSS_VERSION *s)
{
	if(from->size() < 2)
		return 0;
	QByteArray a = ByteStream::takeArray(from, 2);
	s->version = a[0];
	s->method = a[1];
	return 1;
}

// authUsername
static QByteArray spc_set_authUsername(const QCString &user, const QCString &pass)
{
	int len1 = user.length();
	int len2 = pass.length();
	if(len1 > 255)
		len1 = 255;
	if(len2 > 255)
		len2 = 255;
	QByteArray a(1+1+len1+1+len2);
	a[0] = 0x01; // username auth version 1
	a[1] = len1;
	memcpy(a.data() + 2, user.data(), len1);
	a[2+len1] = len2;
	memcpy(a.data() + 3 + len1, pass.data(), len2);
	return a;
}

static QByteArray sps_set_authUsername(bool success)
{
	QByteArray a(2);
	a[0] = 0x01;
	a[1] = success ? 0x00 : 0xff;
	return a;
}

struct SPCS_AUTHUSERNAME
{
	QString user, pass;
};

static int spc_get_authUsername(QByteArray *from, SPCS_AUTHUSERNAME *s)
{
	if(from->size() < 1)
		return 0;
	unsigned char ver = from->at(0);
	if(ver != 0x01)
		return -1;
	if(from->size() < 2)
		return 0;
	unsigned char ulen = from->at(1);
	if((int)from->size() < ulen + 3)
		return 0;
	unsigned char plen = from->at(ulen+2);
	if((int)from->size() < ulen + plen + 3)
		return 0;
	QByteArray a = ByteStream::takeArray(from, ulen + plen + 3);

	QCString user, pass;
	user.resize(ulen+1);
	pass.resize(plen+1);
	memcpy(user.data(), a.data()+2, ulen);
	memcpy(pass.data(), a.data()+ulen+3, plen);
	s->user = QString::fromUtf8(user);
	s->pass = QString::fromUtf8(pass);
	return 1;
}

struct SPSS_AUTHUSERNAME
{
	unsigned char version;
	bool success;
};

static int sps_get_authUsername(QByteArray *from, SPSS_AUTHUSERNAME *s)
{
	if(from->size() < 2)
		return 0;
	QByteArray a = ByteStream::takeArray(from, 2);
	s->version = a[0];
	s->success = a[1] == 0 ? true: false;
	return 1;
}

// connectRequest
static QByteArray sp_set_connectRequest(const QHostAddress &addr, unsigned short port, unsigned char cmd1=0x01)
{
	int at = 0;
	QByteArray a(4);
	a[at++] = 0x05; // socks version 5
	a[at++] = cmd1;
	a[at++] = 0x00; // reserved
	if(addr.isIp4Addr()) {
		a[at++] = 0x01; // address type = ipv4
		Q_UINT32 ip4 = htonl(addr.ip4Addr());
		a.resize(at+4);
		memcpy(a.data() + at, &ip4, 4);
		at += 4;
	}
	else {
		a[at++] = 0x04;
		Q_UINT8 a6[16];
		QStringList s6 = QStringList::split(':', addr.toString(), true);
		int at = 0;
		Q_UINT16 c;
		bool ok;
		for(QStringList::ConstIterator it = s6.begin(); it != s6.end(); ++it) {
			c = (*it).toInt(&ok, 16);
			a6[at++] = (c >> 8);
			a6[at++] = c & 0xff;
		}
		a.resize(at+16);
		memcpy(a.data() + at, a6, 16);
		at += 16;
	}

	// port
	a.resize(at+2);
	unsigned short p = htons(port);
	memcpy(a.data() + at, &p, 2);

	return a;
}

static QByteArray sp_set_connectRequest(const QString &host, Q_UINT16 port, unsigned char cmd1=0x01)
{
	// detect for IP addresses
	QHostAddress addr;
	if(addr.setAddress(host))
		return sp_set_connectRequest(addr, port, cmd1);

	QString h = host;
	h.truncate(255);

	int at = 0;
	QByteArray a(4);
	a[at++] = 0x05; // socks version 5
	a[at++] = cmd1;
	a[at++] = 0x00; // reserved
	a[at++] = 0x03; // address type = domain

	// host
	a.resize(at+h.length());
	a[at++] = h.length();
	memcpy(a.data() + at, h.latin1(), h.length());
	at += h.length();

	// port
	a.resize(at+2);
	unsigned short p = htons(port);
	memcpy(a.data() + at, &p, 2);

	return a;
}

struct SPS_CONNREQ
{
	unsigned char version;
	unsigned char cmd;
	int address_type;
	QString host;
	QHostAddress addr;
	Q_UINT16 port;
};

static int sp_get_connectRequest(QByteArray *from, SPS_CONNREQ *s)
{
	int full_len = 4;
	if((int)from->size() < full_len)
		return 0;

	QString host;
	QHostAddress addr;
	unsigned char atype = from->at(3);

	if(atype == 0x01) {
		full_len += 4;
		if((int)from->size() < full_len)
			return 0;
		Q_UINT32 ip4;
		memcpy(&ip4, from->data() + 4, 4);
		addr.setAddress(ntohl(ip4));
	}
	else if(atype == 0x03) {
		++full_len;
		if((int)from->size() < full_len)
			return 0;
		unsigned char host_len = from->at(4);
		full_len += host_len;
		if((int)from->size() < full_len)
			return 0;
		QCString cs(host_len+1);
		memcpy(cs.data(), from->data() + 5, host_len);
		host = QString::fromLatin1(cs);
	}
	else if(atype == 0x04) {
		full_len += 16;
		if((int)from->size() < full_len)
			return 0;
		Q_UINT8 a6[16];
		memcpy(a6, from->data() + 4, 16);
		addr.setAddress(a6);
	}

	full_len += 2;
	if((int)from->size() < full_len)
		return 0;

	QByteArray a = ByteStream::takeArray(from, full_len);

	Q_UINT16 p;
	memcpy(&p, a.data() + full_len - 2, 2);

	s->version = a[0];
	s->cmd = a[1];
	s->address_type = atype;
	s->host = host;
	s->addr = addr;
	s->port = ntohs(p);

	return 1;
}

enum { StepVersion, StepAuth, StepRequest };

class SocksClient::Private
{
public:
	Private() {}

	BSocket sock;
	QString host;
	int port;
	QString user, pass;
	QString real_host;
	int real_port;

	QByteArray recvBuf;
	bool active;
	int step;
	int authMethod;
	bool incoming, waiting;

	QString rhost;
	int rport;
};

SocksClient::SocksClient(QObject *parent)
:ByteStream(parent)
{
	init();

	d->incoming = false;
}

SocksClient::SocksClient(int s, QObject *parent)
:ByteStream(parent)
{
	init();

	d->incoming = true;
	d->waiting = true;
	d->sock.setSocket(s);
}

void SocksClient::init()
{
	d = new Private;
	connect(&d->sock, SIGNAL(connected()), SLOT(sock_connected()));
	connect(&d->sock, SIGNAL(connectionClosed()), SLOT(sock_connectionClosed()));
	connect(&d->sock, SIGNAL(delayedCloseFinished()), SLOT(sock_delayedCloseFinished()));
	connect(&d->sock, SIGNAL(readyRead()), SLOT(sock_readyRead()));
	connect(&d->sock, SIGNAL(bytesWritten(int)), SLOT(sock_bytesWritten(int)));
	connect(&d->sock, SIGNAL(error(int)), SLOT(sock_error(int)));

	reset(true);
}

SocksClient::~SocksClient()
{
	reset(true);
	delete d;
}

void SocksClient::reset(bool clear)
{
	if(d->sock.state() != BSocket::Idle)
		d->sock.close();
	if(clear)
		clearReadBuffer();
	d->recvBuf.resize(0);
	d->active = false;
	d->waiting = false;
}

bool SocksClient::isIncoming() const
{
	return d->incoming;
}

void SocksClient::setAuth(const QString &user, const QString &pass)
{
	d->user = user;
	d->pass = pass;
}

void SocksClient::connectToHost(const QString &proxyHost, int proxyPort, const QString &host, int port)
{
	reset(true);

	d->host = proxyHost;
	d->port = proxyPort;
	d->real_host = host;
	d->real_port = port;

#ifdef PROX_DEBUG
	fprintf(stderr, "SocksClient: Connecting to %s:%d", proxyHost.latin1(), proxyPort);
	if(d->user.isEmpty())
		fprintf(stderr, "\n");
	else
		fprintf(stderr, ", auth {%s,%s}\n", d->user.latin1(), d->pass.latin1());
#endif
	d->sock.connectToHost(d->host, d->port);
}

bool SocksClient::isOpen() const
{
	return d->active;
}

void SocksClient::close()
{
	d->sock.close();
	if(d->sock.bytesToWrite() == 0)
		reset();
}

void SocksClient::write(const QByteArray &buf)
{
	if(d->active)
		return d->sock.write(buf);
}

QByteArray SocksClient::read(int bytes)
{
	if(d->active)
		return ByteStream::read(bytes);
	else
		return QByteArray();
}

int SocksClient::bytesAvailable() const
{
	if(d->active)
		return ByteStream::bytesAvailable();
	else
		return 0;
}

int SocksClient::bytesToWrite() const
{
	if(d->active)
		return d->sock.bytesToWrite();
	else
		return 0;
}

void SocksClient::sock_connected()
{
#ifdef PROX_DEBUG
	fprintf(stderr, "SocksClient: Connected\n");
#endif

	d->step = StepVersion;
	d->sock.write(spc_set_version());
}

void SocksClient::sock_connectionClosed()
{
	if(d->active) {
		reset();
		connectionClosed();
	}
	else {
		error(ErrProxyNeg);
	}
}

void SocksClient::sock_delayedCloseFinished()
{
	if(d->active) {
		reset();
		delayedCloseFinished();
	}
}

void SocksClient::sock_readyRead()
{
	QByteArray block = d->sock.read();

	if(!d->active) {
		if(d->incoming)
			processIncoming(block);
		else
			processOutgoing(block);
	}
	else {
		appendRead(block);
		readyRead();
	}
}

void SocksClient::processOutgoing(const QByteArray &block)
{
#ifdef PROX_DEBUG
	// show hex
	fprintf(stderr, "SocksClient: got { ");
	for(int n = 0; n < (int)block.size(); ++n)
		fprintf(stderr, "%02X ", (unsigned char)block[n]);
	fprintf(stderr, " } \n");
#endif
	ByteStream::appendArray(&d->recvBuf, block);

	if(d->step == StepVersion) {
		SPSS_VERSION s;
		int r = sps_get_version(&d->recvBuf, &s);
		if(r == -1) {
			reset(true);
			error(ErrProxyNeg);
			return;
		}
		else if(r == 1) {
			if(s.version != 0x05 || s.method == 0xff) {
#ifdef PROX_DEBUG
				fprintf(stderr, "SocksClient: Method selection failed\n");
#endif
				reset(true);
				error(ErrProxyNeg);
				return;
			}

			QString str;
			if(s.method == 0x00) {
				str = "None";
				d->authMethod = AuthNone;
			}
			else if(s.method == 0x02) {
				str = "Username/Password";
				d->authMethod = AuthUsername;
			}
			else {
#ifdef PROX_DEBUG
				fprintf(stderr, "SocksClient: Server wants to use unknown method '%02x'\n", s.method);
#endif
				reset(true);
				error(ErrProxyNeg);
				return;
			}

			if(d->authMethod == AuthNone) {
				// no auth, go straight to the request
				do_request();
			}
			else if(d->authMethod == AuthUsername) {
				d->step = StepAuth;
#ifdef PROX_DEBUG
				fprintf(stderr, "SocksClient: Authenticating [Username] ...\n");
#endif
				d->sock.write(spc_set_authUsername(d->user.latin1(), d->pass.latin1()));
			}
		}
	}
	if(d->step == StepAuth) {
		if(d->authMethod == AuthUsername) {
			SPSS_AUTHUSERNAME s;
			int r = sps_get_authUsername(&d->recvBuf, &s);
			if(r == -1) {
				reset(true);
				error(ErrProxyNeg);
				return;
			}
			else if(r == 1) {
				if(s.version != 0x01) {
					reset(true);
					error(ErrProxyNeg);
					return;
				}
				if(!s.success) {
					reset(true);
					error(ErrProxyAuth);
					return;
				}

				do_request();
			}
		}
	}
	else if(d->step == StepRequest) {
		SPS_CONNREQ s;
		int r = sp_get_connectRequest(&d->recvBuf, &s);
		if(r == -1) {
			reset(true);
			error(ErrProxyNeg);
			return;
		}
		else if(r == 1) {
			if(s.cmd != 0x00) {
#ifdef PROX_DEBUG
				fprintf(stderr, "SocksClient: << Error >> [%02x]\n", s.cmd);
#endif
				reset(true);
				if(s.cmd == 0x04)
					error(ErrHostNotFound);
				else if(s.cmd == 0x05)
					error(ErrConnectionRefused);
				else
					error(ErrProxyNeg);
				return;
			}

#ifdef PROX_DEBUG
			fprintf(stderr, "SocksClient: << Success >>\n");
#endif
			d->active = true;
			connected();

			if(!d->recvBuf.isEmpty()) {
				appendRead(d->recvBuf);
				d->recvBuf.resize(0);
				readyRead();
			}
		}
	}
}

void SocksClient::do_request()
{
#ifdef PROX_DEBUG
	fprintf(stderr, "SocksClient: Requesting ...\n");
#endif
	d->step = StepRequest;
	d->sock.write(sp_set_connectRequest(d->real_host, d->real_port));
}

void SocksClient::sock_bytesWritten(int x)
{
	if(d->active)
		bytesWritten(x);
}

void SocksClient::sock_error(int x)
{
	if(d->active) {
		reset();
		error(ErrRead);
	}
	else {
		reset(true);
		if(x == BSocket::ErrHostNotFound)
			error(ErrProxyConnect);
		else if(x == BSocket::ErrConnectionRefused)
			error(ErrProxyConnect);
		else if(x == BSocket::ErrRead)
			error(ErrProxyNeg);
	}
}

void SocksClient::serve()
{
	d->waiting = false;
	d->step = StepVersion;
	continueIncoming();
}

void SocksClient::processIncoming(const QByteArray &block)
{
#ifdef PROX_DEBUG
	// show hex
	fprintf(stderr, "SocksClient: got { ");
	for(int n = 0; n < (int)block.size(); ++n)
		fprintf(stderr, "%02X ", (unsigned char)block[n]);
	fprintf(stderr, " } \n");
#endif
	ByteStream::appendArray(&d->recvBuf, block);

	if(!d->waiting)
		continueIncoming();
}

void SocksClient::continueIncoming()
{
	if(d->recvBuf.isEmpty())
		return;

	if(d->step == StepVersion) {
		SPCS_VERSION s;
		int r = spc_get_version(&d->recvBuf, &s);
		if(r == -1) {
			reset(true);
			error(ErrProxyNeg);
			return;
		}
		else if(r == 1) {
			if(s.version != 0x05) {
				reset(true);
				error(ErrProxyNeg);
				return;
			}

			int methods = 0;
			for(int n = 0; n < (int)s.methodList.size(); ++n) {
				unsigned char c = s.methodList[n];
				if(c == 0x00)
					methods |= AuthNone;
				else if(c == 0x02)
					methods |= AuthUsername;
			}
			d->waiting = true;
			incomingMethods(methods);
		}
	}
	else if(d->step == StepAuth) {
		SPCS_AUTHUSERNAME s;
		int r = spc_get_authUsername(&d->recvBuf, &s);
		if(r == -1) {
			reset(true);
			error(ErrProxyNeg);
			return;
		}
		else if(r == 1) {
			d->waiting = true;
			incomingAuth(s.user, s.pass);
		}
	}
	else if(d->step == StepRequest) {
		SPS_CONNREQ s;
		int r = sp_get_connectRequest(&d->recvBuf, &s);
		if(r == -1) {
			reset(true);
			error(ErrProxyNeg);
			return;
		}
		else if(r == 1) {
			d->waiting = true;
			if(!s.host.isEmpty())
				d->rhost = s.host;
			else
				d->rhost = s.addr.toString();
			d->rport = s.port;
			incomingRequest(d->rhost, d->rport);
		}
	}
}

void SocksClient::chooseMethod(int method)
{
	if(d->step != StepVersion || !d->waiting)
		return;

	unsigned char c;
	if(method == AuthNone) {
		d->step = StepRequest;
		c = 0x00;
	}
	else {
		d->step = StepAuth;
		c = 0x02;
	}

	// version response
	d->waiting = false;
	d->sock.write(sps_set_version(c));
	continueIncoming();
}

void SocksClient::authGrant(bool b)
{
	if(d->step != StepAuth || !d->waiting)
		return;

	if(b)
		d->step = StepRequest;

	// auth response
	d->waiting = false;
	d->sock.write(sps_set_authUsername(b));
	if(!b) {
		reset(true);
		return;
	}
	continueIncoming();
}

void SocksClient::requestGrant(bool b)
{
	if(d->step != StepRequest || !d->waiting)
		return;

	// request response
	d->waiting = false;
	unsigned char cmd1;
	if(b)
		cmd1 = 0x00; // success
	else
		cmd1 = 0x04; // host not found
	d->sock.write(sp_set_connectRequest(d->rhost, d->rport, cmd1));
	if(!b) {
		reset(true);
		return;
	}
	d->active = true;

	if(!d->recvBuf.isEmpty()) {
		appendRead(d->recvBuf);
		d->recvBuf.resize(0);
		readyRead();
	}
}


//----------------------------------------------------------------------------
// SocksServer
//----------------------------------------------------------------------------
class SocksServer::Private
{
public:
	Private() {}

	ServSock serv;
	QPtrList<SocksClient> incomingConns;
};

SocksServer::SocksServer(QObject *parent)
:QObject(parent)
{
	d = new Private;
	connect(&d->serv, SIGNAL(connectionReady(int)), SLOT(connectionReady(int)));
}

SocksServer::~SocksServer()
{
	d->incomingConns.setAutoDelete(true);
	d->incomingConns.clear();
	delete d;
}

bool SocksServer::isActive() const
{
	return d->serv.isActive();
}

bool SocksServer::listen(Q_UINT16 port)
{
	return d->serv.listen(port);
}

void SocksServer::stop()
{
	d->serv.stop();
}

int SocksServer::port() const
{
	return d->serv.port();
}

SocksClient *SocksServer::takeIncoming()
{
	if(d->incomingConns.isEmpty())
		return 0;

	SocksClient *c = d->incomingConns.getFirst();
	d->incomingConns.removeRef(c);

	// we don't care about errors anymore
	disconnect(c, SIGNAL(error(int)), this, SLOT(connectionError()));

	// don't serve the connection until the event loop, to give the caller a chance to map signals
	QTimer::singleShot(0, c, SLOT(serve()));

	return c;
}

void SocksServer::connectionReady(int s)
{
	SocksClient *c = new SocksClient(s, this);
	connect(c, SIGNAL(error(int)), this, SLOT(connectionError()));
	d->incomingConns.append(c);
	incomingReady();
}

void SocksServer::connectionError()
{
	SocksClient *c = (SocksClient *)sender();
	d->incomingConns.removeRef(c);
	c->deleteLater();
}

#ifdef CS_NAMESPACE
}
#endif
