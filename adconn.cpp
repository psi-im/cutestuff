#include"adconn.h"

#include<qapplication.h>
#include"util/bconsole.h"
#include"network/bsocket.h"
#include"network/httpconnect.h"

#include<stdio.h>

class App::Private
{
public:
	Private() {}

	BSocket s;
	HttpConnect h;
	BConsole *c;
	int mode;
};

App::App(int mode, const QString &host, int port, const QString &serv, const QString &proxy_host, int proxy_port)
:QObject(0)
{
	d = new Private;
	d->c = new BConsole;
	connect(d->c, SIGNAL(connectionClosed()), SLOT(con_connectionClosed()));
	connect(d->c, SIGNAL(readyRead()), SLOT(con_readyRead()));

	d->mode = mode;

	if(mode == 0 || mode == 1) {
		connect(&d->s, SIGNAL(connected()), SLOT(st_connected()));
		connect(&d->s, SIGNAL(connectionClosed()), SLOT(st_connectionClosed()));
		connect(&d->s, SIGNAL(readyRead()), SLOT(st_readyRead()));
		connect(&d->s, SIGNAL(error(int)), SLOT(st_error(int)));
		if(mode == 0) {
			fprintf(stderr, "adconn: Connecting to %s:%d ...\n", host.latin1(), port);
			d->s.connectToHost(host, port);
		}
		else {
			fprintf(stderr, "adconn: Connecting to '%s' server at %s ...\n", serv.latin1(), host.latin1());
			d->s.connectToServer(host, serv);
		}
	}
	else if(mode == 2) {
		connect(&d->h, SIGNAL(connected()), SLOT(st_connected()));
		connect(&d->h, SIGNAL(connectionClosed()), SLOT(st_connectionClosed()));
		connect(&d->h, SIGNAL(readyRead()), SLOT(st_readyRead()));
		connect(&d->h, SIGNAL(error(int)), SLOT(st_error(int)));
		fprintf(stderr, "adconn: Connecting to %s:%d via %s:%d (https)\n", host.latin1(), port, proxy_host.latin1(), proxy_port);
		d->h.connectToHost(proxy_host, proxy_port, host, port);
	}
}

App::~App()
{
	delete d->c;
	delete d;
}

void App::st_connected()
{
	fprintf(stderr, "adconn: Connected\n");
}

void App::st_connectionClosed()
{
	fprintf(stderr, "adconn: Connection closed by foreign host.\n");
	quit();
}

void App::st_readyRead()
{
	QByteArray a;
	if(d->mode == 0 || d->mode == 1)
		a = d->s.read();
	else if(d->mode == 2)
		a = d->h.read();
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
	if(d->mode == 0 || d->mode == 1)
		d->s.close();
	else if(d->mode == 2)
		d->h.close();

	quit();
}

void App::con_readyRead()
{
	QByteArray a = d->c->read();
	if(d->mode == 0 || d->mode == 1)
		d->s.write(a);
	else if(d->mode == 2)
		d->h.write(a);
}


int main(int argc, char **argv)
{
	QApplication app(argc, argv, false);

	if(argc < 2) {
		printf("usage: adconn [host:port]\n");
		printf("       adconn [server] [server_type]\n");
		printf("       adconn --proxy=[https|poll|socks],[host:port] [host:port]\n");
		printf("\n");
		return 0;
	}

	int mode=-1;
	QString host;
	int port=0;
	QString serv;
	QString proxy_host;
	int proxy_port=0;

	QString s = argv[1];
	if(s.left(2) == "--") {
		QString opt, rest;
		int n = s.find('=', 2);
		if(n != -1) {
			opt = s.mid(2, n-2);
			++n;
			rest = s.mid(n);
		}
		else {
			opt = s.mid(2);
			rest = "";
		}

		if(opt == "proxy") {
			if(argc < 3) {
				fprintf(stderr, "Not enough arguments.\n");
				return 0;
			}

			QString type;
			bool ok = false;
			int n = rest.find(',');
			if(n != -1) {
				type = rest.mid(0, n);
				++n;
				int n2 = rest.find(':', n);
				if(n2 != -1) {
					proxy_host = rest.mid(n, n2-n);
					n = n2 + 1;
					proxy_port = rest.mid(n).toInt();
					ok = true;
				}
			}
			if(!ok) {
				fprintf(stderr, "Bad format of proxy option.\n");
				return 0;
			}

			QString s = argv[2];
			n = s.find(':');
			if(n == -1) {
				fprintf(stderr, "Bad format of host:port.\n");
				return 0;
			}
			host = s.mid(0, n);
			++n;
			port = s.mid(n).toInt();

			if(type == "https") {
				mode = 2;
			}
			else if(type == "poll") {
				fprintf(stderr, "poll proxy mode not supported yet.\n");
				return 0;
			}
			else if(type == "socks") {
				fprintf(stderr, "socks proxy mode not supported yet.\n");
				return 0;
			}
			else {
				fprintf(stderr, "unknown proxy type '%s'\n", type.latin1());
				return 0;
			}
		}
		else {
			fprintf(stderr, "Unknown option '%s'\n", opt.latin1());
			return 0;
		}
	}
	else {
		int n = s.find(':');
		if(n == -1) {
			if(argc < 3) {
				fprintf(stderr, "No server_type specified.\n");
				return 0;
			}
			mode = 1;
			host = s;
			serv = argv[2];
		}
		else {
			mode = 0;
			host = s.mid(0, n);
			port = s.mid(n+1).toInt();
		}
	}

	App *a = new App(mode, host, port, serv, proxy_host, proxy_port);
	QObject::connect(a, SIGNAL(quit()), &app, SLOT(quit()));
	app.exec();
	delete a;

	return 0;
}
