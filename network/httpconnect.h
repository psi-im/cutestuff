#ifndef CS_HTTPCONNECT_H
#define CS_HTTPCONNECT_H

#include"../util/bytestream.h"

class HttpConnect : public ByteStream
{
	Q_OBJECT
public:
	HttpConnect(QObject *parent=0);
	~HttpConnect();

	void setProxy(const QString &host, int port, const QString &user="", const QString &pass="");
	void connectToHost(const QString &, int port);

	// from ByteStream
	bool isOpen() const;
	void close();
	int write(const QByteArray &);
	QByteArray read(int bytes=0);
	int bytesAvailable() const;
	int bytesToWrite() const;

signals:
	void connected();

private:
	class Private;
	Private *d;
};

#endif
