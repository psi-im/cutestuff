#include<qapplication.h>
#include"securesocket/securesocket.h"

int main(int argc, char **argv)
{
	if(argc < 2)
		return 0;

	QApplication app(argc, argv);
	SecureSocket *s = new SecureSocket;
	QObject::connect(s, SIGNAL(error(int)), &app, SLOT(quit()));

	if(argc > 2)
		s->connectToHost(argv[1], atoi(argv[2]));
	else
		s->connectToServer(argv[1], "jabber");

	app.exec();
	delete s;

	return 0;
}

