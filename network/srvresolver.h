#ifndef CS_SRVRESOLVER_H
#define CS_SRVRESOLVER_H

#include<qobject.h>

class SrvResolver : public QObject
{
	Q_OBJECT
public:
	SrvResolver(QObject *parent=0);
	~SrvResolver();

	void resolve(const QString &server, const QString &type, const QString &proto);
	void next();
	void stop();
	bool isBusy() const;

	uint result() const;
	QString resultString() const;
	Q_UINT16 resultPort() const;

signals:
	void resultsReady();

private slots:
	void qdns_done();
	void ndns_done();

private:
	class Private;
	Private *d;
};

#endif
