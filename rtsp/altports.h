/*
 * altports.h - manage alternative UDP port ranges
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

#ifndef ALTPORTS_H
#define ALTPORTS_H

#include <qobject.h>
#include <qcstring.h>
#include <qvaluelist.h>
#include <qhostaddress.h>

class PortRange
{
public:
	PortRange();

	int base, count;

	QString toString() const;
	bool fromString(const QString &s);
};

class PortRangeList : public QValueList<PortRange>
{
public:
	void merge(const PortRange &r);
	int findByBase(int base) const;
};

class AltPorts : public QObject
{
	Q_OBJECT
public:
	AltPorts();
	~AltPorts();

	void reset();
	bool isEmpty() const;

	bool allocate(const PortRange &real, PortRange *alt);
	bool reserve(const PortRangeList &real, PortRangeList *alt);
	void keep(const PortRange &r);
	PortRange range() const;
	void send(int index, const QHostAddress &addr, int destPort, const QByteArray &buf);

signals:
	void packetReady(int index, const QHostAddress &addr, int sourcePort, const QByteArray &buf);

public:
	class Private;
private:
	friend class Private;
	Private *d;
};

#endif
