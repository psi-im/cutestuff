/*
 * socksclient.cpp - SOCKS5 TCP proxy
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

#include"socksclient.h"

#include<qhostaddress.h>
#include<netinet/in.h>
#include"../network/bsocket.h"

#ifdef PROX_DEBUG
#include<stdio.h>
#endif

// SOCKS packet stuff
static QByteArray spc_version()
{
	QByteArray ver(5);
	ver[0] = 0x05; // socks version 5
	ver[1] = 0x03; // number of methods
	ver[2] = 0x00; // no-auth
	ver[3] = 0x01; // gss-api
	ver[4] = 0x02; // username
	return ver;
}

struct SPSS_VERSION
{
	unsigned char version;
	unsigned char method;
};

static bool sps_version(QByteArray *from, SPSS_VERSION *s)
{
	if(from->size() < 2)
		return false;
	QByteArray a = ByteStream::takeArray(from, 2);
	s->version = a[0];
	s->method = a[1];
	return true;
}

static QByteArray spc_authUsername(const QCString &user, const QCString &pass)
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

struct SPSS_AUTHUSERNAME
{
	unsigned char version;
	bool success;
};

static bool sps_authUsername(QByteArray *from, SPSS_AUTHUSERNAME *s)
{
	if(from->size() < 2)
		return false;
	QByteArray a = ByteStream::takeArray(from, 2);
	s->version = a[0];
	s->success = a[1] == 0 ? true: false;
	return true;
}

static QByteArray spc_connectRequest(const QHostAddress &addr, unsigned short port)
{
	int at = 0;
	QByteArray a(4);
	a[at++] = 0x05; // socks version 5
	a[at++] = 0x01; // cmd1 = Connect
	a[at++] = 0x00; // reserved
	if(addr.isIp4Addr()) {
		a[at++] = 0x01; // address type = ipv4
		Q_UINT32 ip4 = htonl(addr.ip4Addr());
		a.resize(at+4);
		memcpy(a.data() + at, &ip4, 4);
		at += 4;
	}
	else {
		// TODO: support requests with IPv6 addresses
	}

	// port
	a.resize(at+2);
	unsigned short p = htons(port);
	memcpy(a.data() + at, &p, 2);

	return a;
}

static QByteArray spc_connectRequest(const QString &host, Q_UINT16 port)
{
	// detect for IP addresses
	QHostAddress addr;
	if(addr.setAddress(host))
		return spc_connectRequest(addr, port);

	QString h = host;
	h.truncate(255);

	int at = 0;
	QByteArray a(4);
	a[at++] = 0x05; // socks version 5
	a[at++] = 0x01; // cmd1 = Connect
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

struct SPSS_REPLY
{
	unsigned char version;
	unsigned char reply;
	int address_type;
	QString host;
	QHostAddress addr;
	Q_UINT16 port;
};

static bool sps_reply(QByteArray *from, SPSS_REPLY *s)
{
	int full_len = 4;
	if((int)from->size() < full_len)
		return false;

	QString host;
	QHostAddress addr;
	unsigned char atype = from->at(3);

	if(atype == 0x01) {
		full_len += 4;
		if((int)from->size() < full_len)
			return false;
		Q_UINT32 ip4;
		memcpy(&ip4, from->data() + 4, 4);
		addr.setAddress(ntohl(ip4));
	}
	else if(atype == 0x03) {
		++full_len;
		if((int)from->size() < full_len)
			return false;
		unsigned char host_len = from->at(4);
		full_len = 1 + host_len;
		if((int)from->size() < full_len)
			return false;
		QCString cs(host_len);
		memcpy(cs.data(), from->data() + 5, host_len);
		host = QString::fromLatin1(cs);
	}
	else if(atype == 0x04) {
		// TODO: support replies with IPv6 addresses
	}

	full_len += 2;
	if((int)from->size() < full_len)
		return false;

	QByteArray a = ByteStream::takeArray(from, full_len);

	Q_UINT16 p;
	memcpy(&p, a.data() + full_len - 2, 2);

	s->version = a[0];
	s->reply = a[1];
	s->address_type = atype;
	s->host = host;
	s->addr = addr;
	s->port = ntohs(p);

	return true;
}

enum { AuthNone, AuthGSSAPI, AuthUsername };
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
};

SocksClient::SocksClient(QObject *parent)
:ByteStream(parent)
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
	if(d->sock.isOpen())
		d->sock.close();
	if(clear) {
		clearReadBuffer();
		d->recvBuf.resize(0);
	}
	d->active = false;
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

int SocksClient::write(const QByteArray &buf)
{
	if(d->active)
		return d->sock.write(buf);
	else
		return -1;
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
	d->sock.write(spc_version());
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
			if(sps_version(&d->recvBuf, &s)) {
				if(s.version != 0x05) {
					// TODO
					return;
				}
				if(s.method == 0xff) {
#ifdef PROX_DEBUG
					fprintf(stderr, "SocksClient: Server does not accept any method\n");
#endif
					// TODO
					return;
				}

				QString str;
				if(s.method == 0) {
					str = "None";
					d->authMethod = AuthNone;
				}
				else if(s.method == 1) {
					str = "GSSAPI";
					d->authMethod = AuthGSSAPI;
				}
				else if(s.method == 2) {
					str = "Username/Password";
					d->authMethod = AuthUsername;
				}
				else {
#ifdef PROX_DEBUG
					str = QString::number(s.method, 16);
					fprintf(stderr, "SocksClient: Server wants to use unknown method '%s'\n", str.latin1());
#endif
					return;
				}

				if(d->authMethod == AuthNone) {
					d->step = StepRequest;
					d->sock.write(spc_connectRequest(d->real_host, d->real_port));
				}
				else if(d->authMethod == AuthUsername) {
					d->step = StepAuth;
#ifdef PROX_DEBUG
					fprintf(stderr, "SocksClient: Authenticating ...\n");
#endif
					d->sock.write(spc_authUsername(d->user.latin1(), d->pass.latin1()));
				}
			}
		}
		if(d->step == StepAuth) {
			if(d->authMethod == AuthUsername) {
				SPSS_AUTHUSERNAME s;
				if(sps_authUsername(&d->recvBuf, &s)) {
					if(s.version != 0x01) {
						// TODO: error
						return;
					}
					if(!s.success) {
						// TODO: failed login
						return;
					}

#ifdef PROX_DEBUG
					fprintf(stderr, "SocksClient: Requesting ...\n");
#endif
					d->step = StepRequest;
					d->sock.write(spc_connectRequest(d->real_host, d->real_port));
				}
			}
		}
		else if(d->step == StepRequest) {
			SPSS_REPLY s;
			if(sps_reply(&d->recvBuf, &s)) {
				if(s.reply != 0x00) {
#ifdef PROX_DEBUG
					fprintf(stderr, "SocksClient: << Error >> [%02x]\n", s.reply);
#endif
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
	else {
		appendRead(block);
		readyRead();
	}
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
