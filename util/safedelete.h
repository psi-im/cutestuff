#ifndef SAFEDELETE_H

#include<qobjectlist.h>

class SafeDelete;
class SafeDeleteLock
{
public:
	SafeDeleteLock(SafeDelete *sd);
	~SafeDeleteLock();

private:
	SafeDelete *_sd;
	bool own;
	friend class SafeDelete;
	void dying();
};

class SafeDelete
{
public:
	SafeDelete();
	~SafeDelete();

	void deleteLater(QObject *o);

private:
	QObjectList list;
	void deleteAll();

	friend class SafeDeleteLock;
	SafeDeleteLock *lock;
	void unlock();
};

#endif
