/*
 * rtspproxy.cpp - proxy for RTSP allowing direct and/or virtual endpoints
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

#include "rtspproxy.h"

#include <qurl.h>
#include "servsock.h"
#include "bsocket.h"
#include "rtspbase.h"
#include "altports.h"

//----------------------------------------------------------------------------
// PortMapper
//----------------------------------------------------------------------------
static PortRangeList create_virtual_ranges(const PortRangeList &in)
{
	PortRangeList out;
	int base = 1;
	for(PortRangeList::ConstIterator it = in.begin(); it != in.end(); ++it)
	{
		PortRange r;
		r.base = base;
		r.count = (*it).count;
		base += r.count;
		out.append(r);
	}
	return out;
}

class MapItem
{
public:
	bool active;
	bool virt;
	QHostAddress host;
	PortRange realPorts;
	AltPorts altPorts;
	PortRangeList realPortRanges, altPortRanges;

	MapItem()
	{
		active = false;
		virt = false;
	}

	void reset()
	{
		active = false;
		virt = false;
		host = QHostAddress();
		altPorts.reset();
	}
};

class PortMapper : public QObject
{
	Q_OBJECT
public:
	PortMapper();
	~PortMapper();

	void reset();

	// setup
	bool reserveDirectClient(const PortRangeList &clientRanges, bool virtServer=false);
	bool reserveVirtualClient(const PortRangeList &clientRanges);
	bool finalize(const QHostAddress &clientHost, const PortRange &clientPorts, const QHostAddress &serverHost, const PortRange &serverPorts);

	PortRangeList clientAlternatePorts() const;
	PortRangeList serverAlternatePorts() const;

	void writeAsClient(int source, int dest, const QByteArray &buf);
	void writeAsServer(int source, int dest, const QByteArray &buf);

signals:
	void packetFromClient(int source, int dest, const QByteArray &buf);
	void packetFromServer(int source, int dest, const QByteArray &buf);

private slots:
	void client_packetReady(int index, const QHostAddress &addr, int sourcePort, const QByteArray &buf);
	void server_packetReady(int index, const QHostAddress &addr, int sourcePort, const QByteArray &buf);

private:
	MapItem client, server;
	bool ready;
};

PortMapper::PortMapper()
{
	connect(&client.altPorts, SIGNAL(packetReady(int, const QHostAddress &, int, const QByteArray &)), SLOT(client_packetReady(int, const QHostAddress &, int, const QByteArray &)));
	connect(&server.altPorts, SIGNAL(packetReady(int, const QHostAddress &, int, const QByteArray &)), SLOT(server_packetReady(int, const QHostAddress &, int, const QByteArray &)));
	ready = false;
}

PortMapper::~PortMapper()
{
}

void PortMapper::reset()
{
	client.reset();
	server.reset();
	ready = false;
}

bool PortMapper::reserveDirectClient(const PortRangeList &clientRanges, bool virtServer)
{
	if(client.active)
		return true;

	client.realPortRanges = clientRanges;
	if(virtServer)
	{
		client.virt = true; // virtual server means virtual client ports
		client.altPortRanges = create_virtual_ranges(clientRanges);
	}
	else
	{
		if(!client.altPorts.reserve(clientRanges, &client.altPortRanges))
			return false;
	}
	client.active = true;

	return true;
}

bool PortMapper::reserveVirtualClient(const PortRangeList &clientRanges)
{
	if(client.active)
		return false;

	server.virt = true; // virtual client means virtual server ports
	client.realPortRanges = clientRanges;
	client.altPorts.reserve(clientRanges, &client.altPortRanges);
	client.active = true;

	return true;
}

bool PortMapper::finalize(const QHostAddress &clientHost, const PortRange &clientPorts, const QHostAddress &serverHost, const PortRange &serverPorts)
{
	if(ready)
		return true;

	int n = client.altPortRanges.findByBase(clientPorts.base);
	if(n == -1)
		return false;

	client.host = clientHost;
	client.realPorts = client.realPortRanges[n];
	client.realPorts.count = clientPorts.count;

	if(client.virt)
	{
		PortRange virtPorts = client.altPortRanges[n];
		virtPorts.count = clientPorts.count;
		client.altPortRanges.clear();
		client.altPortRanges.append(virtPorts);
	}
	else
	{
		client.altPorts.keep(client.realPorts);
		client.altPortRanges.clear();
		client.altPortRanges.append(client.altPorts.range());
	}

	server.host = serverHost;
	server.realPorts = serverPorts;

	if(server.virt)
	{
		PortRangeList in;
		in.append(server.realPorts);
		server.altPortRanges = create_virtual_ranges(in);
	}
	else
	{
		PortRange r;
		if(!server.altPorts.allocate(server.realPorts, &r))
			return false;
		server.altPortRanges.clear();
		server.altPortRanges.append(r);
	}
	server.active = true;

	ready = true;
	return true;
}

PortRangeList PortMapper::clientAlternatePorts() const
{
	return client.altPortRanges;
}

PortRangeList PortMapper::serverAlternatePorts() const
{
	return server.altPortRanges;
}

void PortMapper::writeAsClient(int source, int, const QByteArray &buf)
{
	if(source < client.realPorts.base || source >= client.realPorts.base + client.realPorts.count)
		return;
	int index = source - client.realPorts.base;
	client.altPorts.send(index, server.host, server.realPorts.base + index, buf);
}

void PortMapper::writeAsServer(int source, int, const QByteArray &buf)
{
	if(source < server.realPorts.base || source >= server.realPorts.base + server.realPorts.count)
		return;
	int index = source - server.realPorts.base;
	server.altPorts.send(index, client.host, client.realPorts.base + index, buf);
}

void PortMapper::client_packetReady(int index, const QHostAddress &, int, const QByteArray &buf)
{
	if(server.virt)
		emit packetFromServer(server.altPortRanges.first().base + index, client.realPorts.base + index, buf);
	else
		server.altPorts.send(index, client.host, client.realPorts.base + index, buf);
}

void PortMapper::server_packetReady(int index, const QHostAddress &, int, const QByteArray &buf)
{
	if(client.virt)
		emit packetFromClient(client.altPortRanges.first().base + index, server.realPorts.base + index, buf);
	else
		client.altPorts.send(index, server.host, server.realPorts.base + index, buf);
}

//----------------------------------------------------------------------------
// Session
//----------------------------------------------------------------------------
using namespace RTSP;

static PortRangeList transport_get_ports(const TransportList &list, const QString &type)
{
	PortRangeList out;
	for(TransportList::ConstIterator it = list.begin(); it != list.end(); ++it)
	{
		PortRange r;
		if(!r.fromString((*it).argument(type)))
			continue;
		out.merge(r);
	}
	return out;
}

static TransportList transport_set_ports(const TransportList &_list, const QString &type, const PortRangeList &newpl)
{
	TransportList list = _list;
	PortRangeList oldpl = transport_get_ports(_list, type);
	for(TransportList::Iterator it = list.begin(); it != list.end(); ++it)
	{
		Transport &t = *it;
		PortRange r;
		if(!r.fromString(t.argument(type)))
			continue;
		int n = oldpl.findByBase(r.base);
		if(n == -1)
			continue;
		r.base = newpl[n].base;
		t.setArgument(type, r.toString());
	}
	return list;
}

static PortRangeList transport_get_client_ports(const TransportList &list)
{
	return transport_get_ports(list, "client_port");
}

static TransportList transport_set_client_ports(const TransportList &_list, const PortRangeList &newpl)
{
	return transport_set_ports(_list, "client_port", newpl);
}

static PortRangeList transport_get_server_ports(const TransportList &list)
{
	return transport_get_ports(list, "server_port");
}

static TransportList transport_set_server_ports(const TransportList &_list, const PortRangeList &newpl)
{
	return transport_set_ports(_list, "server_port", newpl);
}

static void showPacket(const RTSP::Packet &p)
{
	if(p.type() == RTSP::Packet::Request)
	{
		printf("--- RTSP Request     ---\n");
		printf("Command:  [%s]\n", p.command().latin1());
		printf("Resource: [%s]\n", p.resource().latin1());
		printf("Version:  [%s]\n", p.version().latin1());
		printf("Headers:\n");
		HeaderList headers = p.headers();
		for(HeaderList::ConstIterator it = headers.begin(); it != headers.end(); ++it)
			printf("  [%s] = [%s]\n", (*it).name.latin1(), (*it).value.latin1());
		QByteArray data = p.data();
		if(!data.isEmpty())
			printf("[%d bytes of attached content]\n", data.size());
		printf("------------------------\n");
	}
	else if(p.type() == RTSP::Packet::Response)
	{
		printf("--- RTSP Response    ---\n");
		printf("Code:     [%d]\n", p.responseCode());
		printf("String:   [%s]\n", p.responseString().latin1());
		printf("Version:  [%s]\n", p.version().latin1());
		printf("Headers:\n");
		HeaderList headers = p.headers();
		for(HeaderList::ConstIterator it = headers.begin(); it != headers.end(); ++it)
			printf("  [%s] = [%s]\n", (*it).name.latin1(), (*it).value.latin1());
		QByteArray data = p.data();
		if(!data.isEmpty())
			printf("[%d bytes of attached content]\n", data.size());
		printf("------------------------\n");
	}
	else if(p.type() == RTSP::Packet::Data)
	{
		printf("--- RTSP Interleaved ---\n");
		printf("Channel:  [%d]\n", p.channel());
		QByteArray data = p.data();
		if(!data.isEmpty())
			printf("[%d bytes of RTP content]\n", data.size());
		printf("------------------------\n");
	}
}

class Session : public QObject
{
	Q_OBJECT
public:
	bool lastWasSetup;

	Session()
	{
		client = 0;
		server = 0;
		mangle = false;

		connect(&local, SIGNAL(incomingReady()), SLOT(local_incomingReady()));
	}

	~Session()
	{
		reset();
	}

	void reset()
	{
		delete client;
		client = 0;
		delete server;
		server = 0;
	}

	/*bool startIncoming(const QStringList &_urls, ByteStream *_server, int *incomingPort)
	{
		urls = _urls;
		server = new RTSP::Server;
		server->setByteStream(_server, RTSP::Client::MServer);
		local.start(8080);
		*incomingPort = 8080;
	}*/

	bool startIncoming(const QValueList<QUrl> &_urls, const QString &serverHost, int serverPort, int *incomingPort)
	{
		urls = _urls;
		shost = serverHost;
		sport = serverPort;
		if(!urls.isEmpty())
			mangle = true;
		if(!local.start(8080))
			return false;
		*incomingPort = 8080;
		return true;
	}

	/*bool startExisting(const QStringList &urls, ByteStream *client, ByteStream *server)
	{
	}

	bool startExisting(const QStringList &urls, ByteStream *client, const QString &serverHost, int serverPort)
	{
	}*/

signals:
	void done();

private slots:
	void local_incomingReady()
	{
		Client *c = local.takeIncoming();
		if(!c)
			return;

		//local.stop();
		client = c;
		connect(client, SIGNAL(connectionClosed()), SLOT(client_connectionClosed()));
		connect(client, SIGNAL(packetReady(const Packet &)), SLOT(client_packetReady(const Packet &)));
		connect(client, SIGNAL(packetWritten()), SLOT(client_packetWritten()));
		connect(client, SIGNAL(error(int)), SLOT(client_error(int)));
	}

	void client_connectionClosed()
	{
		printf("Session: Client: connectionClosed\n");
		delete client;
		client = 0;
	}

	void client_packetReady(const Packet &p)
	{
		showPacket(p);

		// mangle the packet
		lastWasSetup = false;
		if(mangle)
		{
			Packet m = p;

			QUrl u = urls.first();
			int u_port = u.hasPort() ? u.port() : 554;

			QUrl pu(m.resource());
			pu.setHost(u.host());
			pu.setPort(u_port == 554 ? -1 : u_port);

			m.setResource(pu.toString());

			QString cmd = m.command();
			if(cmd == "SETUP")
			{
				TransportList list = m.transports();
				PortRangeList pl = transport_get_client_ports(list);
				printf("SETUP ports [%d]:\n", pl.count());
				for(PortRangeList::ConstIterator it = pl.begin(); it != pl.end(); ++it)
					printf("[%d-%d] ", (*it).base, (*it).count);
				printf("\n");
				mapper.reserveDirectClient(pl, false);
				PortRangeList altPorts = mapper.clientAlternatePorts();
				printf("Alternate ports [%d]:\n", altPorts.count());
				for(PortRangeList::ConstIterator it = altPorts.begin(); it != altPorts.end(); ++it)
					printf("[%d-%d] ", (*it).base, (*it).count);
				printf("\n");
				m.setTransports(transport_set_client_ports(list, altPorts));
				lastWasSetup = true;
			}

			cpackets.append(m);
		}
		else
			cpackets.append(p);

		// on receipt of first packet, connect to server if necessary
		if(!server)
		{
			server = new Client;
			connect(server, SIGNAL(connected()), SLOT(server_connected()));
			connect(server, SIGNAL(connectionClosed()), SLOT(server_connectionClosed()));
			connect(server, SIGNAL(packetReady(const Packet &)), SLOT(server_packetReady(const Packet &)));
			connect(server, SIGNAL(packetWritten()), SLOT(server_packetWritten()));
			connect(server, SIGNAL(error(int)), SLOT(server_error(int)));
			printf("Session: Server: connecting to server\n");
			server->connectToHost(shost, sport);
			return;
		}

		sendPackets();
	}

	void client_packetWritten()
	{
		//printf("Session: Client: packetWritten\n");
	}

	void client_error(int x)
	{
		printf("Session: Client: error %d\n", x);
		delete client;
		client = 0;
	}

	void server_connected()
	{
		printf("Session: Server: connected\n");
		sendPackets();
	}

	void server_connectionClosed()
	{
		printf("Session: Server: connectionClosed\n");
		reset();
	}

	void server_packetReady(const Packet &p)
	{
		showPacket(p);
		if(client)
		{
			Packet m = p;
			if(lastWasSetup)
			{
				TransportList list = m.transports();
				PortRangeList cpl = transport_get_client_ports(list);
				PortRangeList spl = transport_get_server_ports(list);
				printf("SETUP ports [%d]:\n", cpl.count());
				for(PortRangeList::ConstIterator it = cpl.begin(); it != cpl.end(); ++it)
					printf("[%d-%d] ", (*it).base, (*it).count);
				printf("\n");
				printf("Server SETUP ports [%d]:\n", spl.count());
				for(PortRangeList::ConstIterator it = spl.begin(); it != spl.end(); ++it)
					printf("[%d-%d] ", (*it).base, (*it).count);
				printf("\n");

				mapper.finalize(client->peerAddress(), cpl.first(), server->peerAddress(), spl.first());

				PortRangeList altPorts = mapper.serverAlternatePorts();
				printf("Alternate ports [%d]:\n", altPorts.count());
				for(PortRangeList::ConstIterator it = altPorts.begin(); it != altPorts.end(); ++it)
					printf("[%d-%d] ", (*it).base, (*it).count);
				printf("\n");
				m.setTransports(transport_set_server_ports(list, altPorts));
			}
			showPacket(m);
			client->write(m);
		}
	}

	void server_packetWritten()
	{
		//printf("Session: Server: packetWritten\n");
	}

	void server_error(int x)
	{
		printf("Session: Server: error %d\n", x);
		reset();
	}

private:
	void sendPackets()
	{
		for(QValueList<Packet>::Iterator it = cpackets.begin(); it != cpackets.end();)
		{
			showPacket(*it);
			server->write(*it);
			it = cpackets.remove(it);
		}
	}

	QValueList<Packet> cpackets;
	Client *client, *server;
	Server local;
	QValueList<QUrl> urls;
	QString shost;
	int sport;
	bool mangle;
	PortMapper mapper;
};

//----------------------------------------------------------------------------
// RTSPProxy
//----------------------------------------------------------------------------
class RTSPProxy::Private : public QObject
{
	Q_OBJECT
public:
	RTSPProxy *par;

	Private(RTSPProxy *_par) : par(_par)
	{
	}
};

RTSPProxy::RTSPProxy(QObject *parent)
:QObject(parent)
{
	d = new Private(this);
}

RTSPProxy::~RTSPProxy()
{
	delete d;
}

int RTSPProxy::startIncoming(const QStringList &urls, ByteStream *server, int *incomingPort)
{
}

int RTSPProxy::startIncoming(const QStringList &urls, const QString &serverHost, int serverPort, int *incomingPort)
{
	QValueList<QUrl> list;
	for(QStringList::ConstIterator it = urls.begin(); it != urls.end(); ++it)
		list.append(QUrl(*it));
	Session *s = new Session;
	if(!s->startIncoming(list, serverHost, serverPort, incomingPort))
	{
		delete s;
		return -1;
	}
	// TODO: add session to a list or something
}

int RTSPProxy::startExisting(const QStringList &urls, ByteStream *client, ByteStream *server)
{
}

int RTSPProxy::startExisting(const QStringList &urls, ByteStream *client, const QString &serverHost, int serverPort)
{
}

void RTSPProxy::stop(int id)
{
}

void RTSPProxy::writeAsClient(int id, int source, int dest, const QByteArray &buf)
{
}

void RTSPProxy::writeAsServer(int id, int source, int dest, const QByteArray &buf)
{
}

QString RTSPProxy::mangle(const QString &url, const QString &host, int port)
{
	QUrl u(url);
	u.setHost(host);
	u.setPort(port == 554 ? -1 : port);
	return u.toString();
}

#include "rtspproxy.moc"
