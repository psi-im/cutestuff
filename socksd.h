#ifndef TEST_H
#define TEST_H

#include<qobject.h>

class App : public QObject
{
	Q_OBJECT
public:
	App(int, const QString &, const QString &);
	~App();

signals:
	void quit();

private slots:
	void st_connectionClosed();
	void st_delayedCloseFinished();
	void st_readyRead();
	void st_error(int);

	void con_connectionClosed();
	void con_readyRead();

	void ss_incomingReady();
	void sc_incomingMethods(int);
	void sc_incomingAuth(const QString &user, const QString &pass);
	void sc_incomingRequest(const QString &host, int port);
	void sc_error(int);

private:
	class Private;
	Private *d;
};

#endif
