#include"test.h"

#include<qapplication.h>
#include"network/bsocket.h"
#include"network/httpconnect.h"

class App::Private
{
public:
	Private() {}

	HttpConnect h;
};

App::App()
:QObject(0)
{
	d = new Private;

	connect(&d->h, SIGNAL(connected()), SLOT(st_connected()));
	connect(&d->h, SIGNAL(connectionClosed()), SLOT(st_connectionClosed()));
	connect(&d->h, SIGNAL(error(int)), SLOT(st_error(int)));
	d->h.setAuth("psi", "psi");
	d->h.connectToHost("194.254.24.133", 3128, "jabber.org", 5222);
	//d->h.connectToHost("home.homelesshackers.org", 80, "www.google.com", 80);
}

App::~App()
{
	delete d;
}

void App::st_connected()
{
	printf("app: connected\n");
	QCString cs(
	"<?xml version='1.0'?>\n"
	"<stream:stream to='jabber.org' xmlns='jabber:client' xmlns:stream='http://etherx.jabber.org/streams' version='1.0'>\n");
	int len = cs.length();
	QByteArray a(len);
	memcpy(a.data(), cs.data(), len);
	d->h.write(a);
}

void App::st_connectionClosed()
{
	printf("closed.\n");
	quit();
}

void App::st_error(int x)
{
	printf("err: %d\n", x);
	quit();
}


int main(int argc, char **argv)
{
	QApplication app(argc, argv);

	App *a = new App;
	QObject::connect(a, SIGNAL(quit()), &app, SLOT(quit()));
	app.exec();
	delete a;

	return 0;
}

