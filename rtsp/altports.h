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
