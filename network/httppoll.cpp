/*
 * httppoll.cpp - HTTP polling proxy
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

#include"httppoll.h"

#include<qstringlist.h>
#include<qurl.h>
#include<qtimer.h>
#include"../network/bsocket.h"
#include"../util/base64.h"
#include"../util/sha1.h"

#ifdef PROX_DEBUG
#include<stdio.h>
#endif

//----------------------------------------------------------------------------
// HttpPoll
//----------------------------------------------------------------------------
static QString hpk(int n, const QString &s)
{
	if(n == 0)
		return s;
	else
		return Base64::arrayToString( SHA1::hashString(hpk(n - 1, s).latin1()) );
}

class HttpPoll::Private
{
public:
	Private() {}

	HttpProxyPost http;
	QString host;
	int port;
	QString user, pass;
	QString url;

	QByteArray out;

	int state;
	bool closing;
	QString ident;

	QTimer *t;

	QString key[256];
	int key_n;
};

HttpPoll::HttpPoll(QObject *parent)
:ByteStream(parent)
{
	d = new Private;

	d->t = new QTimer;
	connect(d->t, SIGNAL(timeout()), SLOT(do_sync()));

	connect(&d->http, SIGNAL(result()), SLOT(http_result()));
	connect(&d->http, SIGNAL(error(int)), SLOT(http_error(int)));

	reset(true);
}

HttpPoll::~HttpPoll()
{
	reset(true);
	delete d->t;
	delete d;
}

void HttpPoll::reset(bool clear)
{
	if(d->http.isActive())
		d->http.stop();
	if(clear)
		clearReadBuffer();
	clearWriteBuffer();
	d->out.resize(0);
	d->state = 0;
	d->closing = false;
	d->t->stop();
}

void HttpPoll::setAuth(const QString &user, const QString &pass)
{
	d->user = user;
	d->pass = pass;
}

void HttpPoll::connectToHost(const QString &proxyHost, int proxyPort, const QString &url)
{
	reset(true);

	d->host = proxyHost;
	d->port = proxyPort;
	d->url = url;

	resetKey();
	bool last;
	QString key = getKey(&last);

#ifdef PROX_DEBUG
	fprintf(stderr, "HttpPoll: Connecting to %s:%d", proxyHost.latin1(), proxyPort);
	if(d->user.isEmpty())
		fprintf(stderr, "\n");
	else
		fprintf(stderr, ", auth {%s,%s}\n", d->user.latin1(), d->pass.latin1());
#endif
	d->state = 1;
	d->http.setAuth(d->user, d->pass);
	d->http.post(d->host, d->port, d->url, makePacket("0", key, "", QByteArray()));
}

QByteArray HttpPoll::makePacket(const QString &ident, const QString &key, const QString &newkey, const QByteArray &block)
{
	QString str = ident;
	if(!key.isEmpty()) {
		str += ';';
		str += key;
	}
	if(!newkey.isEmpty()) {
		str += ';';
		str += newkey;
	}
	str += ',';
	QCString cs = str.latin1();
	int len = cs.length();

	QByteArray a(len + block.size());
	memcpy(a.data(), cs.data(), len);
	memcpy(a.data() + len, block.data(), block.size());
	return a;
}

bool HttpPoll::isOpen() const
{
	return (d->state == 2 ? true: false);
}

void HttpPoll::close()
{
	if(d->state == 0 || d->closing)
		return;

	if(bytesToWrite() == 0)
		reset();
	else
		d->closing = true;
}

void HttpPoll::http_result()
{
	// get id and packet
	QString id;
	QString cookie = d->http.getHeader("Set-Cookie");
	int n = cookie.find("ID=");
	if(n == -1) {
		reset();
		error(ErrRead);
		return;
	}
	n += 3;
	int n2 = cookie.find(';', n);
	if(n2 != -1)
		id = cookie.mid(n, n2-n);
	else
		id = cookie.mid(n);
	QByteArray block = d->http.body();

	// session error?
	if(id.right(2) == ":0") {
		if(id == "0:0") {
			reset();
			connectionClosed();
			return;
		}
		else {
			error(ErrRead);
			return;
		}
	}

	d->ident = id;

	// connecting
	if(d->state == 1) {
		d->state = 2;
		connected();
	}
	else {
		if(!d->out.isEmpty()) {
			int x = d->out.size();
			d->out.resize(0);
			takeWrite(x);
			bytesWritten(x);
		}
	}

	if(!block.isEmpty()) {
		appendRead(block);
		readyRead();
	}

	if(bytesToWrite() > 0) {
		do_sync();
	}
	else {
		if(d->closing) {
			reset();
			delayedCloseFinished();
			return;
		}

		// sync up again in 1 minute
		d->t->start(30000, true);
	}
}

void HttpPoll::http_error(int x)
{
	reset();
	if(x == HttpProxyPost::ErrConnectionRefused)
		error(ErrConnectionRefused);
	else if(x == HttpProxyPost::ErrHostNotFound)
		error(ErrHostNotFound);
	else if(x == HttpProxyPost::ErrSocket)
		error(ErrRead);
	else if(x == HttpProxyPost::ErrProxyConnect)
		error(ErrProxyConnect);
	else if(x == HttpProxyPost::ErrProxyNeg)
		error(ErrProxyNeg);
	else if(x == HttpProxyPost::ErrProxyAuth)
		error(ErrProxyAuth);
}

int HttpPoll::tryWrite()
{
	if(!d->http.isActive())
		do_sync();
	return 0;
}

void HttpPoll::do_sync()
{
	d->t->stop();
	d->out = takeWrite(0, false);

	bool last;
	QString key = getKey(&last);
	QString newkey;
	if(last) {
		resetKey();
		newkey = getKey(&last);
	}
	d->http.post(d->host, d->port, d->url, makePacket(d->ident, key, newkey, d->out));
}

void HttpPoll::resetKey()
{
	fprintf(stderr, "reset key!!\n");
	d->key_n = 256;
	for(int n = 0; n < 256; ++n)
		d->key[n] = hpk(n+1, "foo");
}

const QString & HttpPoll::getKey(bool *last)
{
	*last = false;
	--(d->key_n);
	if(d->key_n == 0)
		*last = true;
	return d->key[d->key_n];
}


//----------------------------------------------------------------------------
// HttpProxyPost
//----------------------------------------------------------------------------
static QString extractLine(QByteArray *buf, bool *found)
{
	// scan for newline
	int n;
	for(n = 0; n < (int)buf->size()-1; ++n) {
		if(buf->at(n) == '\r' && buf->at(n+1) == '\n') {
			QCString cstr;
			cstr.resize(n+1);
			memcpy(cstr.data(), buf->data(), n);
			n += 2; // hack off CR/LF

			memmove(buf->data(), buf->data() + n, buf->size() - n);
			buf->resize(buf->size() - n);
			QString s = QString::fromUtf8(cstr);

			if(found)
				*found = true;
			return s;
		}
	}

	if(found)
		*found = false;
	return "";
}

static bool extractMainHeader(const QString &line, QString *proto, int *code, QString *msg)
{
	int n = line.find(' ');
	if(n == -1)
		return false;
	if(proto)
		*proto = line.mid(0, n);
	++n;
	int n2 = line.find(' ', n);
	if(n2 == -1)
		return false;
	if(code)
		*code = line.mid(n, n2-n).toInt();
	n = n2+1;
	if(msg)
		*msg = line.mid(n);
	return true;
}

class HttpProxyPost::Private
{
public:
	Private() {}

	BSocket sock;
	QByteArray postdata, recvBuf, body;
	QString url;
	QString user, pass;
	bool inHeader;
	QStringList headerLines;
};

HttpProxyPost::HttpProxyPost(QObject *parent)
:QObject(parent)
{
	d = new Private;
	connect(&d->sock, SIGNAL(connected()), SLOT(sock_connected()));
	connect(&d->sock, SIGNAL(connectionClosed()), SLOT(sock_connectionClosed()));
	connect(&d->sock, SIGNAL(readyRead()), SLOT(sock_readyRead()));
	connect(&d->sock, SIGNAL(error(int)), SLOT(sock_error(int)));
	reset(true);
}

HttpProxyPost::~HttpProxyPost()
{
	reset(true);
	delete d;
}

void HttpProxyPost::reset(bool clear)
{
	if(d->sock.state() != BSocket::Idle)
		d->sock.close();
	d->recvBuf.resize(0);
	if(clear)
		d->body.resize(0);
}

void HttpProxyPost::setAuth(const QString &user, const QString &pass)
{
	d->user = user;
	d->pass = pass;
}

bool HttpProxyPost::isActive() const
{
	return (d->sock.state() == BSocket::Idle ? false: true);
}

void HttpProxyPost::post(const QString &proxyHost, int proxyPort, const QString &url, const QByteArray &data)
{
	reset(true);

	d->url = url;
	d->postdata = data;

#ifdef PROX_DEBUG
	fprintf(stderr, "HttpProxyPost: Connecting to %s:%d", proxyHost.latin1(), proxyPort);
	if(d->user.isEmpty())
		fprintf(stderr, "\n");
	else
		fprintf(stderr, ", auth {%s,%s}\n", d->user.latin1(), d->pass.latin1());
#endif
	d->sock.connectToHost(proxyHost, proxyPort);
}

void HttpProxyPost::stop()
{
	reset();
}

QByteArray HttpProxyPost::body() const
{
	return d->body;
}

QString HttpProxyPost::getHeader(const QString &var) const
{
	for(QStringList::ConstIterator it = d->headerLines.begin(); it != d->headerLines.end(); ++it) {
		const QString &s = *it;
		int n = s.find(": ");
		if(n == -1)
			continue;
		QString v = s.mid(0, n);
		if(v == var)
			return s.mid(n+2);
	}
	return "";
}

void HttpProxyPost::sock_connected()
{
#ifdef PROX_DEBUG
	fprintf(stderr, "HttpProxyPost: Connected\n");
#endif
	d->inHeader = true;
	d->headerLines.clear();

	QUrl u = d->url;

	// connected, now send the request
	QString s;
	s += QString("POST ") + d->url + " HTTP/1.0\r\n";
	if(!d->user.isEmpty()) {
		QString str = d->user + ':' + d->pass;
		s += QString("Proxy-Authorization: Basic ") + Base64::encodeString(str) + "\r\n";
	}
	s += "Proxy-Connection: Keep-Alive\r\n";
	s += "Pragma: no-cache\r\n";
	s += QString("Host: ") + u.host() + "\r\n";
	s += "Content-Type: application/x-www-form-urlencoded\r\n";
	s += QString("Content-Length: ") + QString::number(d->postdata.size()) + "\r\n";
	s += "\r\n";

	// write request
	QCString cs = s.utf8();
	QByteArray block(cs.length());
	memcpy(block.data(), cs.data(), block.size());
	d->sock.write(block);

	// write postdata
	d->sock.write(d->postdata);
}

void HttpProxyPost::sock_connectionClosed()
{
	d->body = d->recvBuf.copy();
	reset();
	result();
}

void HttpProxyPost::sock_readyRead()
{
	QByteArray block = d->sock.read();
	ByteStream::appendArray(&d->recvBuf, block);

	if(d->inHeader) {
		// grab available lines
		while(1) {
			bool found;
			QString line = extractLine(&d->recvBuf, &found);
			if(!found)
				break;
			if(line.isEmpty()) {
				d->inHeader = false;
				break;
			}
			d->headerLines += line;
		}

		// done with grabbing the header?
		if(!d->inHeader) {
			QString str = d->headerLines.first();
			d->headerLines.remove(d->headerLines.begin());

			QString proto;
			int code;
			QString msg;
			if(!extractMainHeader(str, &proto, &code, &msg)) {
#ifdef PROX_DEBUG
				fprintf(stderr, "HttpProxyPost: invalid header!\n");
#endif
				reset(true);
				error(ErrProxyNeg);
				return;
			}
			else {
#ifdef PROX_DEBUG
				fprintf(stderr, "HttpProxyPost: header proto=[%s] code=[%d] msg=[%s]\n", proto.latin1(), code, msg.latin1());
				for(QStringList::ConstIterator it = d->headerLines.begin(); it != d->headerLines.end(); ++it)
					fprintf(stderr, "HttpProxyPost: * [%s]\n", (*it).latin1());
#endif
			}

			if(code == 200) { // OK
#ifdef PROX_DEBUG
				fprintf(stderr, "HttpProxyPost: << Success >>\n");
#endif
			}
			else {
				int err;
				QString errStr;
				if(code == 407) { // Authentication failed
					err = ErrProxyAuth;
					errStr = tr("Authentication failed");
				}
				else if(code == 404) { // Host not found
					err = ErrHostNotFound;
					errStr = tr("Host not found");
				}
				else if(code == 403) { // Access denied
					err = ErrProxyNeg;
					errStr = tr("Access denied");
				}
				else if(code == 503) { // Connection refused
					err = ErrConnectionRefused;
					errStr = tr("Connection refused");
				}
				else { // invalid reply
					err = ErrProxyNeg;
					errStr = tr("Invalid reply");
				}

#ifdef PROX_DEBUG
				fprintf(stderr, "HttpProxyPost: << Error >> [%s]\n", errStr.latin1());
#endif
				reset(true);
				error(err);
				return;
			}
		}
	}
}

void HttpProxyPost::sock_error(int x)
{
	reset(true);
	if(x == BSocket::ErrHostNotFound)
		error(ErrProxyConnect);
	else if(x == BSocket::ErrConnectionRefused)
		error(ErrProxyConnect);
	else if(x == BSocket::ErrRead)
		error(ErrProxyNeg);
}