/*
 * altports.cpp - manage alternative UDP port ranges
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

#include "altports.h"

#include <qsocketdevice.h>
#include <qsocketnotifier.h>
#include <qptrlist.h>

//----------------------------------------------------------------------------
// PortRange
//----------------------------------------------------------------------------
PortRange::PortRange()
{
	base = 0;
	count = 0;
}

QString PortRange::toString() const
{
	QString s;
	if(count > 0)
	{
		s += QString::number(base);
		if(count > 1)
		{
			s += '-';
			s += QString::number(base + count - 1);
		}
	}
	return s;
}

bool PortRange::fromString(const QString &s)
{
	if(s.isEmpty())
		return false;

	int n = s.find('-');
	if(n == -1)
	{
		// single port
		base = s.toInt();
		count = 1;
	}
	else
	{
		// port range
		int first = s.mid(0, n).toInt();
		int last = s.mid(n + 1).toInt();
		if(first < 1 || first > 65535 || last < first || last > 65535)
			return false;

		base = first;
		count = last - first + 1;
	}
	return true;
}

//----------------------------------------------------------------------------
// PortRangeList
//----------------------------------------------------------------------------
void PortRangeList::merge(const PortRange &r)
{
	// see if we have this base already
	for(Iterator it = begin(); it != end(); ++it)
	{
		PortRange &pr = *it;
		if(pr.base == r.base)
		{
			pr.count = QMAX(pr.count, r.count);
			return;
		}
	}

	// else, just append it
	append(r);
}

int PortRangeList::findByBase(int base) const
{
	int index = 0;
	for(ConstIterator it = begin(); it != end(); ++it)
	{
		if((*it).base == base)
			return index;
		++index;
	}
	return -1;
}

//----------------------------------------------------------------------------
// UDPItem
//----------------------------------------------------------------------------
class UDPItem : public QObject
{
	Q_OBJECT
public:
	static UDPItem *create(const QHostAddress &addr, int port)
	{
		QSocketDevice *sd = new QSocketDevice(QSocketDevice::Datagram);
		sd->setBlocking(false);
		if(!sd->bind(addr, port))
			return 0;
		UDPItem *i = new UDPItem;
		i->sd = sd;
		i->sn = new QSocketNotifier(i->sd->socket(), QSocketNotifier::Read);
		i->connect(i->sn, SIGNAL(activated(int)), SLOT(sn_activated(int)));
		i->_port = port;
		printf("UDP BIND: [%d]\n", port);
		return i;
	}

	~UDPItem()
	{
		delete sn;
		delete sd;
		printf("UDP UNBIND: [%d]\n", _port);
	}

	int port() const
	{
		return _port;
	}

	void write(const QByteArray &buf, const QHostAddress &addr, int port)
	{
		sd->setBlocking(true);
		sd->writeBlock(buf.data(), buf.size(), addr, port);
		sd->setBlocking(false);
	}

signals:
	void packetReady(const QByteArray &buf, const QHostAddress &addr, int port);

private slots:
	void sn_activated(int)
	{
		QByteArray buf(8192);
		int actual = sd->readBlock(buf.data(), buf.size());
		buf.resize(actual);
		QHostAddress pa = sd->peerAddress();
		int pp = sd->peerPort();
		packetReady(buf, pa, pp);
	}

private:
	UDPItem()
	{
	}

	QSocketDevice *sd;
	QSocketNotifier *sn;
	int _port;
};

//----------------------------------------------------------------------------
// PortRange
//----------------------------------------------------------------------------
#define PORT_ALLOC_BASE 16000
#define PORT_ALLOC_MAX  65535

class PortSequence : public QObject
{
	Q_OBJECT
public:
	PortSequence()
	{
		list.setAutoDelete(true);
	}

	void reset()
	{
		list.clear();
	}

	bool allocate(const QHostAddress &address, int count, int base=-1)
	{
		return try_allocate(address, count, base);
	}

	bool resize(int count)
	{
		// don't allow growing
		if(count > (int)list.count())
			return false;

		QPtrListIterator<UDPItem> it(list);
		it += count;
		for(UDPItem *u; (u = it.current());)
			list.removeRef(u);
		return true;
	}

	void send(int index, const QHostAddress &addr, int destPort, const QByteArray &buf)
	{
		if(index >= 0 && index < (int)list.count())
			list.at(index)->write(buf, addr, destPort);
	}

	QHostAddress address() const
	{
		return addr;
	}

	int base() const
	{
		if(!list.isEmpty())
			return list.getFirst()->port();
		else
			return -1;
	}

	int count() const
	{
		return list.count();
	}

signals:
	void packetReady(int index, const QHostAddress &addr, int port, const QByteArray &buf);

private slots:
	void udp_packetReady(const QByteArray &buf, const QHostAddress &addr, int port)
	{
		UDPItem *su = (UDPItem *)sender();
		bool found = false;
		int index = 0;
		QPtrListIterator<UDPItem> it(list);
		for(UDPItem *u; (u = it.current()); ++it)
		{
			if(u == su)
			{
				found = true;
				break;
			}
			++index;
		}
		if(!found)
			return;

		packetReady(index, addr, port, buf);
	}

private:
	QHostAddress addr;
	QPtrList<UDPItem> list;

	bool try_allocate(const QHostAddress &address, int count, int base)
	{
		reset();
		if(count < 1)
			return true;

		int start = base != -1 ? base : PORT_ALLOC_BASE;
		while(start + (count - 1) <= PORT_ALLOC_MAX)
		{
			QPtrList<UDPItem> udplist;
			bool ok = true;
			for(int n = 0; n < count; ++n)
			{
				UDPItem *i = UDPItem::create(address, start + n);
				if(!i)
				{
					ok = false;
					break;
				}
				udplist.append(i);
			}
			if(ok)
			{
				list = udplist;
				break;
			}
			udplist.setAutoDelete(true);

			// if using 'start', we only get one chance
			if(base != -1)
				break;

			start += udplist.count() + 1;
		}

		if(list.isEmpty())
			return false;

		addr = address;

		QPtrListIterator<UDPItem> it(list);
		for(UDPItem *u; (u = it.current()); ++it)
			connect(u, SIGNAL(packetReady(const QByteArray &, const QHostAddress &, int)), SLOT(udp_packetReady(const QByteArray &, const QHostAddress &, int)));

		return true;
	}
};

//----------------------------------------------------------------------------
// AltPorts
//----------------------------------------------------------------------------
class AltPorts::Private : public QObject
{
	Q_OBJECT
public:
	AltPorts *par;
	QPtrList<PortSequence> list;
	PortSequence *ports;
	PortRangeList orig;

	Private(AltPorts *_par)
	{
		par = _par;
		ports = 0;
	}

public slots:
	void range_packetReady(int index, const QHostAddress &addr, int port, const QByteArray &buf)
	{
		par->packetReady(index, addr, port, buf);
	}
};

AltPorts::AltPorts()
{
	d = new Private(this);
}

AltPorts::~AltPorts()
{
	delete d;
}

void AltPorts::reset()
{
	d->list.clear();
	delete d->ports;
	d->ports = 0;
	d->orig.clear();
}

bool AltPorts::isEmpty() const
{
	if(!d->ports && d->list.isEmpty())
		return true;
	return false;
}

bool AltPorts::allocate(const PortRange &real, PortRange *alt)
{
	if(!isEmpty())
		return false;

	PortRangeList in, out;
	in.append(real);
	if(!reserve(in, &out))
		return false;
	keep(real);
	*alt = out.first();
	return true;
}

bool AltPorts::reserve(const PortRangeList &real, PortRangeList *alt)
{
	if(!isEmpty())
		return false;

	PortRangeList out;
	QPtrList<PortSequence> list;
	for(PortRangeList::ConstIterator it = real.begin(); it != real.end(); ++it)
	{
		PortSequence *s = new PortSequence;
		if(!s->allocate(QHostAddress(), (*it).count))
		{
			delete s;
			list.setAutoDelete(true);
			return false;
		}
		PortRange r;
		r.base = s->base();
		r.count = s->count();
		out.append(r);
		list.append(s);
	}

	d->list = list;
	d->orig = real;
	*alt = out;
	return true;
}

void AltPorts::keep(const PortRange &r)
{
	if(d->ports)
		return;

	int n = d->orig.findByBase(r.base);
	if(n == -1)
		return;

	PortSequence *s = d->list.at(n);
	s->resize(r.count);
	d->ports = s;
	d->list.removeRef(s);
	d->list.setAutoDelete(true);
	d->list.clear();
	d->list.setAutoDelete(false);
	connect(d->ports, SIGNAL(packetReady(int, const QHostAddress &, int, const QByteArray &)), d, SLOT(range_packetReady(int, const QHostAddress &, int, const QByteArray &)));
}

PortRange AltPorts::range() const
{
	PortRange r;
	if(d->ports)
	{
		r.base = d->ports->base();
		r.count = d->ports->count();
	}
	return r;
}

void AltPorts::send(int index, const QHostAddress &addr, int destPort, const QByteArray &buf)
{
	d->ports->send(index, addr, destPort, buf);
}

#include "altports.moc"
