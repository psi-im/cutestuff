/*
 * rtspbase.cpp - very basic RTSP client/server
 * Copyright (C) 2004  Justin Karneges
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

#include "rtspbase.h"

#include <qtimer.h>
#include <qguardedptr.h>
#include "bsocket.h"
#include "servsock.h"

#ifdef Q_OS_WIN
# include <windows.h>
#else
# include <netinet/in.h>
#endif

#define MAX_CONTENT_LENGTH 32767

namespace RTSP {

//----------------------------------------------------------------------------
// HeaderList
//----------------------------------------------------------------------------
HeaderList::HeaderList()
:QValueList<Var>()
{
}

bool HeaderList::has(const QString &var) const
{
	QString low = var.lower();
	for(ConstIterator it = begin(); it != end(); ++it)
	{
		if((*it).name.lower() == low)
			return true;
	}
	return false;
}

QString HeaderList::get(const QString &var) const
{
	QString low = var.lower();
	for(ConstIterator it = begin(); it != end(); ++it)
	{
		if((*it).name.lower() == low)
			return (*it).value;
	}
	return QString::null;
}

void HeaderList::set(const QString &var, const QString &val)
{
	QString low = var.lower();
	for(Iterator it = begin(); it != end(); ++it)
	{
		if((*it).name.lower() == low)
		{
			(*it).value = val;
			return;
		}
	}
	Var v;
	v.name = var;
	v.value = val;
	append(v);
}

void HeaderList::remove(const QString &var)
{
	QString low = var.lower();
	for(Iterator it = begin(); it != end(); ++it)
	{
		if((*it).name.lower() == low)
		{
			QValueList<Var>::remove(it);
			return;
		}
	}
}

//----------------------------------------------------------------------------
// Transport
//----------------------------------------------------------------------------
static int findOneOf(const QString &s, const QString &list, int i, QChar *found)
{
	for(int n = i; n < (int)s.length(); ++n)
	{
		for(int k = 0; k < (int)list.length(); ++k)
		{
			if(s[n] == list[k])
			{
				if(found)
					*found = list[k];
				return n;
			}
		}
	}
	return -1;
}

Transport::Transport()
{
}

QString Transport::name() const
{
	return _name;
}

TransportArgs Transport::arguments() const
{
	return _args;
}

QString Transport::argument(const QString &name) const
{
	for(TransportArgs::ConstIterator it = _args.begin(); it != _args.end(); ++it)
	{
		const Var &v = *it;
		if(v.name == name)
			return v.value;
	}
	return QString::null;
}

void Transport::setArguments(const TransportArgs &list)
{
	_args = list;
}

void Transport::setArgument(const QString &name, const QString &value)
{
	for(TransportArgs::Iterator it = _args.begin(); it != _args.end(); ++it)
	{
		Var &v = *it;
		if(v.name == name)
		{
			v.value = value;
			return;
		}
	}
}

QString Transport::toString() const
{
	QString s;
	s += _name;
	if(!_args.isEmpty())
	{
		for(TransportArgs::ConstIterator it = _args.begin(); it != _args.end(); ++it)
		{
			s += ';';
			s += (*it).name;
			if(!(*it).value.isNull())
			{
				s += '=';
				s += (*it).value;
			}
		}
	}
	return s;
}

Transport::ReadStatus Transport::readFromString(QString *sp)
{
	const QString &s = *sp;
	QString tname;
	QValueList<Var> targs;

	// first read the name
	int at = 0;
	QChar c;
	int n = findOneOf(s, ",;", at, &c);
	if(n == -1)
	{
		tname = s.mid(at);
		// empty at end means nothing left
		if(tname.isEmpty())
			return Done;

		at = s.length();
	}
	else if(c == ',')
	{
		tname = s.mid(at, n - at);
		// empty with separator is bad
		if(tname.isEmpty())
			return Error;

		at = n + 1;
	}
	else if(c == ';')
	{
		tname = s.mid(at, n - at);
		// empty with separator is bad
		if(tname.isEmpty())
			return Error;

		at = n + 1;

		// read args
		QString targ, tval;
		bool last = false;
		while(!last)
		{
			// get arg
			n = findOneOf(s, ",;=", at, &c);
			if(n == -1)
			{
				targ = s.mid(at);
				// empty at end means nothing left
				if(targ.isEmpty())
					break;

				at = s.length();
				last = true;
			}
			else if(c == ',')
			{
				targ = s.mid(at, n - at);
				// empty with separator is bad
				if(targ.isEmpty())
					return Error;

				at = n + 1;
				last = true;
			}
			else if(c == ';')
			{
				targ = s.mid(at, n - at);
				// empty with separator is bad
				if(targ.isEmpty())
					return Error;

				at = n + 1;
			}
			else if(c == '=')
			{
				// this arg has a value
				targ = s.mid(at, n - at);
				// empty with value is bad
				if(targ.isEmpty())
					return Error;

				at = n + 1;

				// is next char a quote?
				if(at < (int)s.length() && s[at] == '\"')
				{
					n = s.find('\"', at + 1);
					if(n == -1)
						return Error;
					tval = s.mid(at, n - at + 1);
					at = n + 1;

					if(at < (int)s.length())
					{
						// if not at end, the next char better be a separator
						if(s[at] != ',' && s[at] != ';')
							return Error;

						if(s[at] == ',')
							last = true;

						// skip over it
						++at;
					}
					else
						last = true;
				}
				else
				{
					// find next separator
					n = findOneOf(s, ",;", at, &c);
					if(n == -1)
					{
						tval = s.mid(at);
						at = s.length();
						last = true;
					}
					else if(c == ',')
					{
						tval = s.mid(at, n - at);
						at = n + 1;
						last = true;
					}
					else if(c == ';')
					{
						tval = s.mid(at, n - at);
						at = n + 1;
					}
				}
			}

			Var v;
			v.name = targ;
			v.value = tval;
			targs.append(v);
		}
	}

	_name = tname;
	_args = targs;

	// remove what we just parsed
	(*sp) = sp->mid(at);
	return Ok;
}

//----------------------------------------------------------------------------
// TransportList
//----------------------------------------------------------------------------
TransportList::TransportList()
:QValueList<Transport>()
{
}

QString TransportList::toString() const
{
	QStringList list;
	for(ConstIterator it = begin(); it != end(); ++it)
		list.append((*it).toString());
	return list.join(",");
}

bool TransportList::fromString(const QString &s)
{
	QString tmp = s;
	clear();

	while(1)
	{
		Transport t;
		Transport::ReadStatus r = t.readFromString(&tmp);
		if(r == Transport::Error)
			return false;
		if(r == Transport::Done)
			break;
		append(t);
	}

	return true;
}

//----------------------------------------------------------------------------
// Packet
//----------------------------------------------------------------------------
Packet::Packet()
{
	t = Empty;
}

Packet::Packet(const QString &command, const QString &resource, const HeaderList &headers)
{
	cmd = command;
	res = resource;
	_headers = headers;
	ver = "1.0";
}

Packet::Packet(int responseCode, const QString &responseString, const HeaderList &headers)
{
	rcode = responseCode;
	rstr = responseString;
	_headers = headers;
	ver = "1.0";
}

Packet::~Packet()
{
}

bool Packet::isNull() const
{
	return (t == Empty);
}

Packet::Type Packet::type() const
{
	return t;
}

QString Packet::version() const
{
	return ver;
}

QString Packet::command() const
{
	return cmd;
}

QString Packet::resource() const
{
	return res;
}

int Packet::responseCode() const
{
	return rcode;
}

QString Packet::responseString() const
{
	return rstr;
}

int Packet::channel() const
{
	return chan;
}

QByteArray Packet::data() const
{
	return _data;
}

HeaderList & Packet::headers()
{
	return _headers;
}

const HeaderList & Packet::headers() const
{
	return _headers;
}

TransportList Packet::transports() const
{
	TransportList list;
	list.fromString(_headers.get("Transport"));
	return list;
}

void Packet::setTransports(const TransportList &list)
{
	_headers.set("Transport", list.toString());
}

void Packet::setResource(const QString &s)
{
	res = s;
}

QByteArray Packet::toArray() const
{
	QByteArray buf;

	if(t == Data)
	{
		buf.resize(4 + _data.size());

		buf[0] = '$';
		buf[1] = chan;
		ushort ssa = _data.size();
		ushort ssb = htons(ssa);
		memcpy(buf.data() + 2, &ssb, 2);
		memcpy(buf.data() + 4, _data.data(), _data.size());
	}
	else
	{
		QString str;

		if(t == Request)
		{
			str += cmd + ' ' + res + " RTSP/" + ver + '\n';
			for(HeaderList::ConstIterator it = _headers.begin(); it != _headers.end(); ++it)
				str += (*it).name + ": " + (*it).value + '\n';
			str += '\n';
		}
		else if(t == Response)
		{
			str += "RTSP/" + ver + ' ' + QString::number(rcode) + ' ' + rstr + '\n';
			for(HeaderList::ConstIterator it = _headers.begin(); it != _headers.end(); ++it)
				str += (*it).name + ": " + (*it).value + '\n';
			str += '\n';
		}

		QCString cs = str.utf8();
		buf.resize(cs.length());
		memcpy(buf.data(), cs.data(), buf.size());
		ByteStream::appendArray(&buf, _data);
	}

	return buf;
}

//----------------------------------------------------------------------------
// Parser
//----------------------------------------------------------------------------
Parser::Parser()
{
}

void Parser::reset(Mode _mode)
{
	mode = _mode;
	in.resize(0);
	cur.clear();
	tmp = Packet();
	list.clear();
}

void Parser::appendData(const QByteArray &a)
{
	ByteStream::appendArray(&in, a);
}

Packet Parser::read(bool *ok)
{
	if(ok)
		*ok = true;

	if(list.isEmpty())
	{
		if(!readPacket())
		{
			if(ok)
				*ok = false;
		}
	}

	Packet p;
	if(!list.isEmpty())
	{
		p = list.first();
		list.remove(list.begin());
	}
	return p;
}

bool Parser::readPacket()
{
	bool interleaved = false;
	bool done = false;

	// don't know what it is yet?  let's have a look
	if(tmp.t == Packet::Empty)
	{
		// need at least 1 byte
		if(in.isEmpty())
			return true;

		if(in[0] == '$')
		{
			// interleaved data
			if(in.size() < 4)
				return true;
			ushort ss;
			memcpy(&ss, in.data() + 2, 2);
			int size = ntohs(ss);
			if((int)in.size() < 4 + size)
				return true;

			QByteArray buf = ByteStream::takeArray(&in, 4 + size);
			tmp.t = Packet::Data;
			tmp.chan = buf[1];
			tmp._data.resize(size);
			memcpy(tmp._data.data(), buf.data() + 4, size);

			interleaved = true;
			done = true;
		}
		else
		{
			// regular request/response
			if(mode == Client)
				tmp.t = Packet::Request;
			else
				tmp.t = Packet::Response;
			readingContent = false;
		}
	}

	if(!interleaved)
	{
		if(!readingContent)
		{
			// read the lines
			QStringList lines = readPacketLines();
			if(lines.isEmpty())
				return true;
			if(!packetFromLines(lines))
				return false;
			if(tmp._headers.has("Content-length"))
			{
				clen = tmp._headers.get("Content-length").toInt();
				if(clen > MAX_CONTENT_LENGTH)
					return false;
			}
			else
				clen = 0;
			readingContent = true;
		}

		if(readingContent)
		{
			if(clen > 0)
			{
				int need = clen - tmp._data.size();
				QByteArray a;
				if((int)in.size() >= need)
					a = ByteStream::takeArray(&in, need);
				else
					a = ByteStream::takeArray(&in);
				ByteStream::appendArray(&tmp._data, a);
			}

			if(clen == 0 || (int)tmp._data.size() == clen)
				done = true;
		}
	}

	if(done)
	{
		list.append(tmp);
		tmp = Packet();
	}

	return true;
}

bool Parser::packetFromLines(const QStringList &lines)
{
	if(mode == Client)
	{
		if(lines.isEmpty())
			return false;
		QString str = lines[0];
		int n = str.find(' ');
		if(n == -1)
			return false;
		QString c = str.mid(0, n);
		int at = n + 1;
		n = str.find(' ', at);
		if(n == -1)
			return false;
		QString r = str.mid(at, n - at);
		QString v = str.mid(n + 1);

		if(v.left(5) != "RTSP/")
			return false;

		HeaderList headers;
		if(!readHeaders(lines, &headers))
			return false;

		tmp.cmd = c;
		tmp.res = r;
		tmp.ver = v.mid(5);
		tmp._headers = headers;
		return true;
	}
	else
	{
		if(lines.isEmpty())
			return false;
		QString str = lines[0];
		int n = str.find(' ');
		if(n == -1)
			return false;
		QString v = str.mid(0, n);
		int at = n + 1;
		n = str.find(' ', at);
		if(n == -1)
			return false;
		QString c = str.mid(at, n - at);
		QString s = str.mid(n + 1);
		if(v.left(5) != "RTSP/")
			return false;

		HeaderList headers;
		if(!readHeaders(lines, &headers))
			return false;

		tmp.ver = v.mid(5);
		tmp.rcode = c.toInt();
		tmp.rstr = s;
		tmp._headers = headers;
		return true;
	}
}

bool Parser::readHeaders(const QStringList &lines, HeaderList *headers)
{
	// skip the first line
	QStringList::ConstIterator it = lines.begin();
	++it;

	headers->clear();
	for(; it != lines.end(); ++it)
	{
		const QString &s = *it;
		int n = s.find(':');
		if(n == -1)
			return false;
		Var v;
		v.name = s.mid(0, n).stripWhiteSpace();
		v.value = s.mid(n + 1).stripWhiteSpace();
		headers->append(v);
	}
	return true;
}

QStringList Parser::readPacketLines()
{
	QStringList lines;
	while(1)
	{
		QString str = tryReadLine();
		if(str.isNull())
			break;

		if(str.isEmpty())
		{
			lines = cur;
			cur.clear();
			break;
		}

		cur.append(str);
	}
	return lines;
}

QString Parser::tryReadLine()
{
	for(int n = 0; n < (int)in.size(); ++n)
	{
		if(in[n] == '\n')
		{
			int eat = n + 1;
			if(n > 0 && in[n-1] == '\r')
				--n;
			QByteArray buf = ByteStream::takeArray(&in, eat);
			QCString cs;
			cs.resize(n + 1);
			memcpy(cs.data(), buf.data(), n);
			return QString::fromUtf8(cs);
		}
	}
	return QString::null;
}

//----------------------------------------------------------------------------
// Client
//----------------------------------------------------------------------------
class Client::Private
{
public:
	Private()
	{
		bs = 0;
		using_sock = false;
		conn = false;
		active = false;
	}

	ByteStream *bs;
	bool using_sock;
	bool conn;
	bool active;
	Parser parser;
	QValueList<int> trackQueue;
};

Client::Client(QObject *parent)
:QObject(parent)
{
	d = new Private;
}

Client::Client(int socket)
{
	d = new Private;
	d->using_sock = true;
	BSocket *sock = new BSocket;
	d->bs = sock;
	d->conn = false;
	hook();
	d->parser.reset(Parser::Client); // as a server, we want to parse client requests
	sock->setSocket(socket);
}

Client::~Client()
{
	delete d->bs;
	delete d;
}

void Client::hook()
{
	connect(d->bs, SIGNAL(connectionClosed()), SLOT(bs_connectionClosed()));
	connect(d->bs, SIGNAL(readyRead()), SLOT(bs_readyRead()));
	connect(d->bs, SIGNAL(bytesWritten(int)), SLOT(bs_bytesWritten(int)));
	connect(d->bs, SIGNAL(error(int)), SLOT(bs_error(int)));
}

void Client::processPackets()
{
	QGuardedPtr<QObject> self = this;
	while(1)
	{
		bool ok;
		Packet p = d->parser.read(&ok);
		if(!ok)
		{
			delete d->bs;
			d->bs = 0;
			error(ErrParse);
			return;
		}

		if(p.isNull())
			break;

		packetReady(p);
		if(!self)
			return;
	}
}

void Client::connectToHost(const QString &host, int port)
{
	BSocket *sock = new BSocket;
	connect(sock, SIGNAL(connected()), SLOT(sock_connected()));
	d->bs = sock;
	d->conn = true;
	hook();
	sock->connectToHost(host, port);
}

void Client::setByteStream(ByteStream *bs, Mode mode)
{
	d->bs = bs;
	d->conn = false;
	hook();
	d->parser.reset(mode == MClient ? Parser::Server : Parser::Client);
}

void Client::close()
{
	if(d->bs)
		d->bs->close();
}

void Client::write(const Packet &p)
{
	QByteArray buf = p.toArray();
	d->trackQueue.append(buf.size());
	d->bs->write(buf);
}

QHostAddress Client::peerAddress() const
{
	QHostAddress addr;
	if(d->using_sock)
		addr = ((BSocket *)d->bs)->peerAddress();
	return addr;
}

void Client::sock_connected()
{
	d->using_sock = true;
	d->active = true;
	d->parser.reset(Parser::Server); // as a client, we want to parse server responses
	connected();
}

void Client::bs_connectionClosed()
{
	connectionClosed();
}

void Client::bs_readyRead()
{
	QByteArray buf = d->bs->read();
	d->parser.appendData(buf);
	if(d->active)
		processPackets();
}

void Client::bs_bytesWritten(int bytes)
{
	int written = 0;
	for(QValueList<int>::Iterator it = d->trackQueue.begin(); it != d->trackQueue.end();)
	{
		int &i = *it;

		// enough bytes?
		if(bytes < i) {
			i -= bytes;
			break;
		}
		bytes -= i;
		it = d->trackQueue.remove(it);
		++written;
	}

	for(int n = 0; n < written; ++n)
		packetWritten();
}

void Client::bs_error(int x)
{
	delete d->bs;
	d->bs = 0;

	if(d->conn)
	{
		if(x == BSocket::ErrConnectionRefused || x == BSocket::ErrHostNotFound)
		{
			error(ErrConnect);
			return;
		}
	}

	error(ErrStream);
	return;
}

void Client::serve()
{
	d->active = true;
	processPackets();
}

//----------------------------------------------------------------------------
// Server
//----------------------------------------------------------------------------
class Server::Private
{
public:
	Private() {}

	ServSock serv;
	QPtrList<Client> incomingConns;
};

Server::Server(QObject *parent)
:QObject(parent)
{
	d = new Private;
	connect(&d->serv, SIGNAL(connectionReady(int)), SLOT(connectionReady(int)));
}

Server::~Server()
{
	d->incomingConns.setAutoDelete(true);
	d->incomingConns.clear();
	delete d;
}

bool Server::isActive() const
{
	return d->serv.isActive();
}

bool Server::start(int port)
{
	return d->serv.listen(port);
}

void Server::stop()
{
	d->serv.stop();
}

int Server::port() const
{
	return d->serv.port();
}

QHostAddress Server::address() const
{
	return d->serv.address();
}

Client *Server::takeIncoming()
{
	if(d->incomingConns.isEmpty())
		return 0;

	Client *c = d->incomingConns.getFirst();
	d->incomingConns.removeRef(c);

	// we don't care about errors anymore
	disconnect(c, SIGNAL(error(int)), this, SLOT(connectionError()));

	// don't serve the connection until the event loop, to give the caller a chance to map signals
	QTimer::singleShot(0, c, SLOT(serve()));

	return c;
}

void Server::connectionReady(int s)
{
	Client *c = new Client(s);
	connect(c, SIGNAL(error(int)), this, SLOT(connectionError()));
	d->incomingConns.append(c);
	incomingReady();
}

void Server::connectionError()
{
	Client *c = (Client *)sender();
	d->incomingConns.removeRef(c);
	c->deleteLater();
}

}
