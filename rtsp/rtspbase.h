/*
 * rtspbase.h - very basic RTSP client/server
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

// TODO:
//  - only listen on the local interface (or let the caller specify the interface)
//  - client should not emit server replies unless the client actually sent a packet,
//    to keep things in sync
//  - if content-length maximum is exceeded, signal this fact?

#ifndef RTSPBASE_H
#define RTSPBASE_H

#include <qstring.h>
#include <qcstring.h>
#include <qstringlist.h>
#include <qvaluelist.h>
#include <qobject.h>
#include <qhostaddress.h>

class ByteStream;

namespace RTSP
{
	class Parser;
	class Server;

	struct Var
	{
		QString name, value;
	};

	// This should probably be a QMap, but it isn't, so there.
	class HeaderList : public QValueList<Var>
	{
	public:
		HeaderList();

		bool has(const QString &var) const;
		QString get(const QString &var) const;
		void set(const QString &var, const QString &val);
		void remove(const QString &var);
	};

	typedef QValueList<Var> TransportArgs;
	class Transport
	{
	public:
		enum ReadStatus { Ok, Done, Error };
		Transport();

		QString name() const;
		TransportArgs arguments() const;
		QString argument(const QString &name) const;

		void setArguments(const TransportArgs &list);
		void setArgument(const QString &name, const QString &value);

		QString toString() const;
		ReadStatus readFromString(QString *s);

	private:
		QString _name;
		TransportArgs _args;
	};

	class TransportList : public QValueList<Transport>
	{
	public:
		TransportList();

		QString toString() const;
		bool fromString(const QString &s);
	};

	class Packet
	{
	public:
		enum Type { Empty, Request, Response, Data };
		Packet();
		Packet(const QString &command, const QString &resource, const HeaderList &headers=HeaderList());
		Packet(int responseCode, const QString &responseString, const HeaderList &headers=HeaderList());
		~Packet();

		bool isNull() const;
		Type type() const;

		QString version() const;
		QString command() const;
		QString resource() const;
		int responseCode() const;
		QString responseString() const;
		int channel() const;
		QByteArray data() const;

		HeaderList & headers();
		const HeaderList & headers() const;

		TransportList transports() const;
		void setTransports(const TransportList &list);

		void setResource(const QString &s);

		QByteArray toArray() const;

	private:
		friend class Parser;
		Type t;
		QString ver, cmd, res, rstr;
		int rcode, chan;
		QByteArray _data;
		HeaderList _headers;
	};

	class Parser
	{
	public:
		enum Mode { Client, Server };
		Parser();

		void reset(Mode mode);
		void appendData(const QByteArray &a);
		Packet read(bool *ok=0);

	private:
		Mode mode;
		QByteArray in;
		QStringList cur;
		Packet tmp;
		QValueList<Packet> list;
		int clen;
		bool readingContent;

		bool readPacket();
		bool packetFromLines(const QStringList &lines);
		bool readHeaders(const QStringList &lines, HeaderList *headers);
		QStringList readPacketLines();
		QString tryReadLine();
	};

	class Client : public QObject
	{
		Q_OBJECT
	public:
		enum Mode { MClient, MServer };
		enum Error { ErrConnect, ErrParse, ErrStream };
		Client(QObject *parent=0);
		~Client();

		void connectToHost(const QString &host, int port);
		void setByteStream(ByteStream *bs, Mode mode);
		void close();

		void write(const Packet &p);

		QHostAddress peerAddress() const;

	signals:
		void connected();
		void connectionClosed();
		void packetReady(const Packet &p);
		void packetWritten();
		void error(int);

	private slots:
		void sock_connected();
		void bs_connectionClosed();
		void bs_readyRead();
		void bs_bytesWritten(int);
		void bs_error(int);
		void serve();

	private:
		class Private;
		Private *d;

		friend class Server;
		Client(int socket);
		void hook();
		void processPackets();
	};

	class Server : public QObject
	{
		Q_OBJECT
	public:
		Server(QObject *parent=0);
		~Server();

		bool isActive() const;
		bool start(int port);
		void stop();
		int port() const;
		QHostAddress address() const;
		Client *takeIncoming();

	signals:
		void incomingReady();

	private slots:
		void connectionReady(int);
		void connectionError();

	private:
		class Private;
		Private *d;
	};
}

#endif
