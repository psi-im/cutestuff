/*
 * rtspproxy.h - proxy for RTSP allowing direct and/or virtual endpoints
 * Copyright (C) 2004  Justin Karneges
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

#ifndef RTSPPROXY_H
#define RTSPPROXY_H

#include <qobject.h>
#include <qcstring.h>
#include <qstringlist.h>
#include "bytestream.h"

class RTSPProxy : public QObject
{
	Q_OBJECT
public:
	RTSPProxy(QObject *parent=0);
	~RTSPProxy();

	int startIncoming(const QStringList &urls, ByteStream *server, int *incomingPort);
	int startIncoming(const QStringList &urls, const QString &serverHost, int serverPort, int *incomingPort);
	int startExisting(const QStringList &urls, ByteStream *client, ByteStream *server);
	int startExisting(const QStringList &urls, ByteStream *client, const QString &serverHost, int serverPort);
	void stop(int id);

	void writeAsClient(int id, int source, int dest, const QByteArray &buf);
	void writeAsServer(int id, int source, int dest, const QByteArray &buf);

	static QString mangle(const QString &url, const QString &host, int port);

signals:
	void packetFromClient(int id, int source, int dest, const QByteArray &buf);
	void packetFromServer(int id, int source, int dest, const QByteArray &buf);

public:
	class Private;
private:
	Private *d;
};

#endif
