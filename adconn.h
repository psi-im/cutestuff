#ifndef TEST_H
#define TEST_H

#include<qobject.h>

class App : public QObject
{
	Q_OBJECT
public:
	App(int mode, const QString &host, int port, const QString &serv, const QString &proxy_host, int proxy_port, const QString &proxy_user, const QString &proxy_pass);
	~App();

signals:
	void quit();

private slots:
	void st_connected();
	void st_connectionClosed();
	void st_delayedCloseFinished();
	void st_readyRead();
	void st_error(int);

	void con_connectionClosed();
	void con_readyRead();

private:
	class Private;
	Private *d;
};

#endif
