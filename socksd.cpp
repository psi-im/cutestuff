#include"socksd.h"

#include<qapplication.h>
#include"bconsole.h"
#include"bsocket.h"
#include"socks.h"

#include<stdio.h>

class App::Private
{
public:
	Private() {}

	ByteStream *bs;
	BConsole *c;
	SocksServer *serv;
	SocksClient *client;

	QString user, pass;
};

App::App(int port, const QString &user, const QString &pass)
:QObject(0)
{
	d = new Private;
	d->user = user;
	d->pass = pass;
	d->c = new BConsole;
	connect(d->c, SIGNAL(connectionClosed()), SLOT(con_connectionClosed()));
	connect(d->c, SIGNAL(readyRead()), SLOT(con_readyRead()));

	d->bs = 0;
	d->client = 0;
	d->serv = new SocksServer;
	connect(d->serv, SIGNAL(incomingReady()), SLOT(ss_incomingReady()));
	d->serv->listen(port);

	fprintf(stderr, "socksd: listening on port %d\n", port);
}

App::~App()
{
	if(d->client)
		d->client->deleteLater();
	delete d->serv;
	delete d->c;
	delete d;
}

void App::st_connectionClosed()
{
	fprintf(stderr, "socksd: Connection closed by foreign host.\n");
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
	fprintf(stderr, "socksd: Stream error [%d].\n", x);
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

void App::ss_incomingReady()
{
	fprintf(stderr, "incoming connection!\n");
	SocksClient *c = d->serv->takeIncoming();
	if(!c)
		return;
	fprintf(stderr, "accepted\n");
	connect(c, SIGNAL(incomingMethods(int)), SLOT(sc_incomingMethods(int)));
	connect(c, SIGNAL(incomingAuth(const QString &, const QString &)), SLOT(sc_incomingAuth(const QString &, const QString &)));
	connect(c, SIGNAL(incomingRequest(const QString &, int)), SLOT(sc_incomingRequest(const QString &, int)));
	connect(c, SIGNAL(error(int)), SLOT(sc_error(int)));
	d->client = c;
}

void App::sc_incomingMethods(int m)
{
	fprintf(stderr, "m=%d\n", m);
	if(d->user.isEmpty() && m & SocksClient::AuthNone)
		d->client->chooseMethod(SocksClient::AuthNone);
	else if(m & SocksClient::AuthUsername)
		d->client->chooseMethod(SocksClient::AuthUsername);
	else {
		d->client->deleteLater();
		d->client = 0;
		fprintf(stderr, "unsupported method!\n");
	}
}

void App::sc_incomingAuth(const QString &user, const QString &pass)
{
	fprintf(stderr, "incoming auth: user=[%s], pass=[%s]\n", user.latin1(), pass.latin1());
	if(user == d->user && pass == d->pass) {
		d->client->authGrant(true);
	}
	else {
		d->client->authGrant(false);
		d->client->deleteLater();
		d->client = 0;
	}
}

void App::sc_incomingRequest(const QString &host, int port)
{
	fprintf(stderr, "request: host=[%s], port=[%d]\n", host.latin1(), port);

	disconnect(d->client, SIGNAL(error(int)), this, SLOT(sc_error(int)));
	connect(d->client, SIGNAL(connectionClosed()), SLOT(st_connectionClosed()));
	connect(d->client, SIGNAL(delayedCloseFinished()), SLOT(st_delayedCloseFinished()));
	connect(d->client, SIGNAL(readyRead()), SLOT(st_readyRead()));
	connect(d->client, SIGNAL(error(int)), SLOT(st_error(int)));
	d->bs = d->client;

	d->client->requestGrant(true);
	fprintf(stderr, "<< Active >>\n");
}

void App::sc_error(int)
{
	d->client->deleteLater();
	d->client = 0;

	fprintf(stderr, "error!\n");
}


int main(int argc, char **argv)
{
	QApplication app(argc, argv, false);

	if(argc < 2) {
		printf("usage: socksd [port] [user] [pass]\n\n");
		return 0;
	}

	QString p = argv[1];
	int port = p.toInt();

	QString user, pass;
	if(argc >= 4) {
		user = argv[2];
		pass = argv[3];
	}

	App *a = new App(port, user, pass);
	QObject::connect(a, SIGNAL(quit()), &app, SLOT(quit()));
	app.exec();
	delete a;

	return 0;
}

