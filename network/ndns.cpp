/*
 * ndns.cpp - native DNS resolution
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

//! \class NDns ndns.h
//! \brief NDns - simple DNS resolution using native system calls
//! 
//!  This class is to be used when Qt's QDns is not good enough.  Because QDns
//!  does not use threads, it cannot make a system call asyncronously.  Thus,
//!  QDns tries to imitate the behavior of each platform's native behavior, and
//!  generally falls short.
//!
//!  NDns uses a thread to make the system call happen in the background.  This
//!  gives your program normal DNS behavior, at the cost of requiring threads
//!  to build.
//!
//!  \code
//!  #include "ndns.h"
//!
//!  ...
//!
//!  NDns dns;
//!  dns.resolve("www.affinix.com");
//!
//!  // The class will emit the resultsReady() signal
//!  // when the resolution is finished. You may then retrieve the results:
//!
//!  uint ip_address = dns.result();
//!
//!  // or if you want to get the string ip,
//!
//!  QString ip_address = dns.resultString();
//!  \endcode

#include "ndns.h"

#ifdef Q_OS_UNIX
#include<netdb.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#endif

#ifdef Q_WS_WIN
#include<windows.h>
#endif

//! \fn void NDns::resultsReady()
//! \brief Status Signal
//!
//! Tells you if the object finished retrieving the ip information.

//! \brief Creates NDns object
//!
//! \param par QObject pointer
NDns::NDns(QObject *par)
:QObject(par)
{
	v_result = 0;
	v_resultString = "";
	worker = 0;
}

//! Destroys NDns Object
NDns::~NDns()
{
}

//! \brief Specify a host to resolve.
//!
//! \param host Host to resolve with the DNS (eg. www.affinix.com)
void NDns::resolve(const QCString &host)
{
	if(worker)
		return;

	worker = new NDnsWorker(this, host);
	worker->start();
}

//! \brief Blocks the current DNS lookup.
//!
//! \note This will not stop the current dns lookup. This will only block the current dns.
void NDns::stop()
{
	worker = 0;
}

//! \brief returns the ip information
//!
//! \return uint - ip address in machine form.\n
//! 0 if the lookup failed.
//! \warning You must wait for resultsReady() to signal back
//! \sa resultsReady()
uint NDns::result() const
{
	return v_result;
}

//! \brief returns the ip in string format
//!
//! \return QString - ip address in string form. \n
//! empty if the lookup failed
//! \warning You must wait for resultsReady() to signal back
//! \sa resultsReady()
QString NDns::resultString() const
{
	return v_resultString;
}

//! \brief Status of the object
//!
//! \return bool - status of your host lookup
bool NDns::isBusy() const
{
	return worker ? true: false;
}

//! Internel function!! (Don't call)
bool NDns::event(QEvent *e)
{
	if(e->type() == QEvent::User) {
		NDnsWorkerEvent *we = (NDnsWorkerEvent *)e;
		if(worker != we->worker())
			return true;

		worker->wait();
		if(worker->success) {
			v_result = worker->addr;
			v_resultString = worker->addrString;
		}
		else {
			v_result = 0;
			v_resultString = "";
		}
		delete worker;
		worker = 0;

		resultsReady();

		return true;
	}
	return false;
}

static QMutex wm;

NDnsWorkerEvent::NDnsWorkerEvent(NDnsWorker *_p)
:QEvent(QEvent::User)
{
	p = _p;
}

NDnsWorker *NDnsWorkerEvent::worker() const
{
	return p;
}


NDnsWorker::NDnsWorker(QObject *_par, const QCString &_host)
{
	success = false;
	par = _par;
	host = _host;
}

void NDnsWorker::run()
{
	// lock during the gethostbyname call (anything that returns data into a static buffer is obviously not thread-safe)
	wm.lock();
	//printf("resolving [%s]\n", host.data());
	hostent *h;
	h = gethostbyname(host.data());
	//printf("done.\n");
	if(!h) {
		//printf("error\n");
		wm.unlock();
		success = false;
		QThread::postEvent(par, new NDnsWorkerEvent(this));
		return;
	}

	in_addr a = *((struct in_addr *)h->h_addr_list[0]);
	addr = ntohl(a.s_addr);
	addrString = inet_ntoa(a);

	//printf("success: [%s]\n", addrString.latin1());
	wm.unlock();
	success = true;
	QThread::postEvent(par, new NDnsWorkerEvent(this));
}

