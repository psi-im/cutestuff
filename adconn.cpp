#include"adconn.h"

#include<qapplication.h>
#include"util/bconsole.h"
#include"network/bsocket.h"
#include"network/httpconnect.h"
#include"network/socksclient.h"
#include"sasl/qsasl.h"
#include"util/base64.h"

#include<stdio.h>

class App::Private
{
public:
	Private() {}

	ByteStream *bs;
	BConsole *c;
	int mode;
	QSASL sasl;
};

App::App(int mode, const QString &host, int port, const QString &serv, const QString &proxy_host, int proxy_port, const QString &proxy_user, const QString &proxy_pass)
:QObject(0)
{
	d = new Private;
	d->c = new BConsole;
	connect(d->c, SIGNAL(connectionClosed()), SLOT(con_connectionClosed()));
	connect(d->c, SIGNAL(readyRead()), SLOT(con_readyRead()));
	d->bs = 0;

	d->mode = mode;

	if(mode == 0 || mode == 1) {
		BSocket *s = new BSocket;
		d->bs = s;
		connect(s, SIGNAL(connected()), SLOT(st_connected()));
		connect(s, SIGNAL(error(int)), SLOT(st_error(int)));
		if(mode == 0) {
			fprintf(stderr, "adconn: Connecting to %s:%d ...\n", host.latin1(), port);
			s->connectToHost(host, port);
		}
		else {
			fprintf(stderr, "adconn: Connecting to '%s' server at %s ...\n", serv.latin1(), host.latin1());
			s->connectToServer(host, serv);
		}
	}
	else if(mode == 2) {
		HttpConnect *s = new HttpConnect;
		d->bs = s;
		connect(s, SIGNAL(connected()), SLOT(st_connected()));
		connect(s, SIGNAL(error(int)), SLOT(st_error(int)));
		fprintf(stderr, "adconn: Connecting to %s:%d via %s:%d (https)\n", host.latin1(), port, proxy_host.latin1(), proxy_port);
		if(!proxy_user.isEmpty())
			s->setAuth(proxy_user, proxy_pass);
		s->connectToHost(proxy_host, proxy_port, host, port);
	}
	else if(mode == 3) {
		SocksClient *s = new SocksClient;
		d->bs = s;
		connect(s, SIGNAL(connected()), SLOT(st_connected()));
		connect(s, SIGNAL(error(int)), SLOT(st_error(int)));
		fprintf(stderr, "adconn: Connecting to %s:%d via %s:%d (socks)\n", host.latin1(), port, proxy_host.latin1(), proxy_port);
		if(!proxy_user.isEmpty())
			s->setAuth(proxy_user, proxy_pass);
		s->connectToHost(proxy_host, proxy_port, host, port);
	}

	/*d->sasl.setServiceHost("jabber", "cataclysm.cx");
	d->sasl.setUserPass("rob", "");
	QStringList list;
	list += "DIGEST-MD5";
	list += "PLAIN";
	d->sasl.begin(list);
	QString str =
	  "cmVhbG09ImNhdGFjbHlzbS5jeCIsbm9uY2U9Ik9BNk1HOXRFUUdtMmhoIi"
	  "xxb3A9ImF1dGgiLGNoYXJzZXQ9dXRmLTgsYWxnb3JpdGhtPW1kNS1zZXNz";
	QByteArray a = Base64::stringToArray(str);
	d->sasl.putChallenge(a);*/
}

App::~App()
{
	delete d->bs;
	delete d->c;
	delete d;
}

void App::st_connected()
{
	fprintf(stderr, "adconn: Connected\n");

	// map the signals
	connect(d->bs, SIGNAL(connectionClosed()), SLOT(st_connectionClosed()));
	connect(d->bs, SIGNAL(delayedCloseFinished()), SLOT(st_delayedCloseFinished()));
	connect(d->bs, SIGNAL(readyRead()), SLOT(st_readyRead()));
}

void App::st_connectionClosed()
{
	fprintf(stderr, "adconn: Connection closed by foreign host.\n");
	quit();
}

void App::st_delayedCloseFinished()
{
	quit();
}

void App::st_readyRead()
{
	QByteArray a = d->bs->read();
	d->c->write(a);
}

void App::st_error(int x)
{
	fprintf(stderr, "adconn: Stream error [%d].\n", x);
	quit();
}

void App::con_connectionClosed()
{
	fprintf(stderr, "adconn: Closing.\n");
	d->bs->close();
	if(d->bs->bytesToWrite() == 0)
		quit();
}

void App::con_readyRead()
{
	QByteArray a = d->c->read();
	d->bs->write(a);
}


int main(int argc, char **argv)
{
	QApplication app(argc, argv, false);

	if(argc < 2) {
		printf("usage: adconn [options] [host]\n");
		printf("   [host] must be in the form 'host:port' or 'domain#server'\n");
		printf("   Options:\n");
		printf("     --proxy=[https|poll|socks],host:port\n");
		printf("     --proxy-auth=user,pass\n");
		printf("\n");
		return 0;
	}

	int mode = 0;
	QString proxy_host;
	int proxy_port=0;
	QString proxy_user, proxy_pass;

	for(int at = 1; at < argc; ++at) {
		QString s = argv[at];

		// is it an option?
		if(s.left(2) == "--") {
			QString name;
			QStringList args;
			int n = s.find('=', 2);
			if(n != -1) {
				name = s.mid(2, n-2);
				++n;
				args = QStringList::split(',', s.mid(n), true);
			}
			else {
				name = s.mid(2);
				args.clear();
			}

			// eat the arg
			--argc;
			for(int x = at; x < argc; ++x)
				argv[x] = argv[x+1];
			--at; // don't advance

			// process option
			if(name == "proxy") {
				QString type = args[0];
				QString s = args[1];
				int n = s.find(':');
				if(n == -1) {
					// TODO: error
				}
				proxy_host = s.mid(0, n);
				++n;
				proxy_port = s.mid(n).toInt();

				if(type == "https") {
					mode = 2;
				}
				else if(type == "poll") {
					printf("proxy 'poll' not supported yet.\n");
					return 0;
				}
				else if(type == "socks") {
					mode = 3;
				}
				else {
					printf("no such proxy type '%s'\n", type.latin1());
					return 0;
				}
			}
			else if(name == "proxy-auth") {
				proxy_user = args[0];
				proxy_pass = args[1];
			}
			else {
				printf("Unknown option '%s'\n", name.latin1());
				return 0;
			}
		}
	}

	if(argc < 2) {
		printf("No host specified!\n");
		return 0;
	}

	QString host, serv;
	int port=0;

	QString s = argv[1];
	int n = s.find(':');
	if(n == -1) {
		n = s.find('#');
		if(n == -1) {
			printf("No port or server specified!\n");
			return 0;
		}

		if(mode >= 2) {
			printf("Can't use domain#server with proxy!\n");
			return 0;
		}
		host = s.mid(0, n);
		++n;
		serv = s.mid(n);
		mode = 1;
	}
	else {
		host = s.mid(0, n);
		++n;
		port = s.mid(n).toInt();
	}

	App *a = new App(mode, host, port, serv, proxy_host, proxy_port, proxy_user, proxy_pass);
	QObject::connect(a, SIGNAL(quit()), &app, SLOT(quit()));
	app.exec();
	delete a;

	return 0;
}
