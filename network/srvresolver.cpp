#include"srvresolver.h"

#include<qcstring.h>
#include<qdns.h>
#include"ndns.h"

/*
  TODO:
  if a server hostname can't be resolved, try the next.  report error if none of the remaining servers resolve.
  add license header
*/

static void sortSRVList(QValueList<QDns::Server> &list)
{
	QValueList<QDns::Server> tmp = list;
	list.clear();

	while(!tmp.isEmpty()) {
		QValueList<QDns::Server>::Iterator p = tmp.end();
		for(QValueList<QDns::Server>::Iterator it = tmp.begin(); it != tmp.end(); ++it) {
			if(p == tmp.end())
				p = it;
			else {
				int a = (*it).priority;
				int b = (*p).priority;
				int j = (*it).weight;
				int k = (*p).weight;
				if(a < b || (a == b && j < k))
					p = it;
			}
		}
		list.append(*p);
		tmp.remove(p);
	}
}

class SrvResolver::Private
{
public:
	Private() {}

	QDns *qdns;
	NDns ndns;

	uint result;
	QString resultString;
	Q_UINT16 resultPort;

	QString srv;
	QValueList<QDns::Server> servers;
};

SrvResolver::SrvResolver(QObject *parent)
:QObject(parent)
{
	d = new Private;
	d->qdns = 0;

	connect(&d->ndns, SIGNAL(resultsReady()), SLOT(ndns_done()));
	stop();
}

SrvResolver::~SrvResolver()
{
	stop();
	delete d;
}

void SrvResolver::resolve(const QString &server, const QString &type, const QString &proto)
{
	stop();

	d->srv = QString("_") + type + "._" + proto + '.' + server;
	d->qdns = new QDns;
	connect(d->qdns, SIGNAL(resultsReady()), SLOT(qdns_done()));
	d->qdns->setRecordType(QDns::Srv);
	d->qdns->setLabel(d->srv);
}

void SrvResolver::next()
{
	if(d->servers.isEmpty())
		return;

	d->ndns.resolve(d->servers.first().name.latin1());
}

void SrvResolver::stop()
{
	if(d->qdns) {
		d->qdns->deleteLater();
		d->qdns = 0;
	}
	if(d->ndns.isBusy())
		d->ndns.stop();
	d->result = 0;
	d->resultString = "";
	d->resultPort = 0;
	d->servers.clear();
	d->srv = "";
}

bool SrvResolver::isBusy() const
{
	if(d->qdns || d->ndns.isBusy())
		return true;
	else
		return false;
}

uint SrvResolver::result() const
{
	return d->result;
}

QString SrvResolver::resultString() const
{
	return d->resultString;
}

Q_UINT16 SrvResolver::resultPort() const
{
	return d->resultPort;
}

void SrvResolver::qdns_done()
{
	// apparently we sometimes get this signal even though the results aren't ready
	if(d->qdns->isWorking())
		return;

	// grab the server list and destroy the qdns object
	QValueList<QDns::Server> list;
	if(d->qdns->recordType() == QDns::Srv)
		list = d->qdns->servers();
	delete d->qdns;
	d->qdns = 0;

	if(list.isEmpty()) {
		stop();
		resultsReady();
		return;
	}
	sortSRVList(list);
	d->servers = list;

	// kick it off
	next();
}

void SrvResolver::ndns_done()
{
	uint r = d->ndns.result();
	if(r) {
		d->result = r;
		d->resultString = d->ndns.resultString();
		d->resultPort = d->servers.first().port;
		d->servers.remove(d->servers.begin());
		resultsReady();
	}
	else {
		stop();
		resultsReady();
	}
}
