#include"httpconnect.h"

#include"../network/bsocket.h"


class HttpConnect::Private
{
public:
	Private() {}

	BSocket *sock;
};

HttpConnect::HttpConnect(QObject *parent)
:ByteStream(parent)
{
	d = new Private;
}

HttpConnect::~HttpConnect()
{
	delete d;
}

void HttpConnect::setProxy(const QString &host, int port, const QString &user, const QString &pass)
{
}

void HttpConnect::connectToHost(const QString &, int port)
{
}

bool HttpConnect::isOpen() const
{
}

void HttpConnect::close()
{
}

int HttpConnect::write(const QByteArray &)
{
}

QByteArray HttpConnect::read(int bytes)
{
}

int HttpConnect::bytesAvailable() const
{
}

int HttpConnect::bytesToWrite() const
{
}
