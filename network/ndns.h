/*
 * ndns.h - native DNS resolution
 * Copyright (C) 2001, 2002  Justin Karneges
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef CS_NDNS_H
#define CS_NDNS_H

#include<qobject.h>
#include<qcstring.h>
#include<qthread.h>

class NDnsWorker;

class NDns : public QObject
{
	Q_OBJECT
public:
	NDns(QObject *par=0);
	~NDns();

	void resolve(const QCString &);
	void stop();
	bool isBusy() const;

	uint result() const;
	QString resultString() const;

signals:
	void resultsReady();

//! \if _hide_doc_
protected:
	bool event(QEvent *);
//! \endif

private:
	uint v_result;
	QString v_resultString;
	NDnsWorker *worker;
};


//! \if _hide_doc_
class NDnsWorkerEvent : public QEvent
{
public:
	NDnsWorkerEvent(NDnsWorker *);

	NDnsWorker *worker() const;

private:
	NDnsWorker *p;
};

class NDnsWorker : public QThread
{
public:
	NDnsWorker(QObject *, const QCString &);

	bool success;
	uint addr;
	QString addrString;

protected:
	void run();

private:
	QCString host;
	QObject *par;
};

#endif
//! \endif
