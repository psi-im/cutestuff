#include"safedelete.h"

SafeDelete::SafeDelete()
{
	lock = 0;
}

SafeDelete::~SafeDelete()
{
	if(lock)
		lock->dying();
}

void SafeDelete::deleteLater(QObject *o)
{
	if(!lock)
		o->deleteLater();
	else
		list.append(o);
}

void SafeDelete::unlock()
{
	lock = 0;
	deleteAll();
}

void SafeDelete::deleteAll()
{
	if(list.isEmpty())
		return;

	QObjectListIt it(list);
	for(QObject *o; (o = it.current()); ++it)
		o->deleteLater();
	list.clear();
}

SafeDeleteLock::SafeDeleteLock(SafeDelete *sd)
{
	own = false;
	if(!sd->lock) {
		_sd = sd;
		_sd->lock = this;
	}
	else
		_sd = 0;
}

SafeDeleteLock::~SafeDeleteLock()
{
	if(_sd) {
		_sd->unlock();
		if(own)
			delete _sd;
	}
}

void SafeDeleteLock::dying()
{
	_sd = new SafeDelete(*_sd);
	own = true;
}
