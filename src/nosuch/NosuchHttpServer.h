/*
	Copyright (c) 2011-2013 Tim Thompson <me@timthompson.com>

	Permission is hereby granted, free of charge, to any person obtaining
	a copy of this software and associated documentation files
	(the "Software"), to deal in the Software without restriction,
	including without limitation the rights to use, copy, modify, merge,
	publish, distribute, sublicense, and/or sell copies of the Software,
	and to permit persons to whom the Software is furnished to do so,
	subject to the following conditions:

	The above copyright notice and this permission notice shall be
	included in all copies or substantial portions of the Software.

	Any person wishing to distribute modifications to the Software is
	requested to send the modifications to the original developer so that
	they can be incorporated into the canonical version.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
	EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
	MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
	IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
	ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
	CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
	WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef NSHTTP_H
#define NSHTTP_H

#include "NosuchJSON.h"
#include "NosuchDaemon.h"
#include "cJSON.h"
#include <list>

#define REQUEST_GET 1
#define REQUEST_POST 2

class NosuchSocketConnection;
class NosuchSocket;

class NosuchHttpServer {
public:
	NosuchHttpServer(NosuchJsonListener* jproc, int port, std::string htmldir, int timeout, int idletime);
	NosuchHttpServer::~NosuchHttpServer();
	void Check();
	void SetHtmlDir(std::string d) { _htmldir = d; }
	void RespondToGetOrPost(NosuchSocketConnection*);
	void InitializeWebSocket(NosuchSocketConnection *kd);
	void CloseWebSocket(NosuchSocketConnection *kd);
	void WebSocketMessage(NosuchSocketConnection *kdata, std::string msg);
	void SendAllWebSocketClients(std::string msg);
	void Shutdown();
	void SetShutdownComplete(bool);
	bool IsShutdownComplete();
	void SetShouldBeShutdown(bool);
	bool ShouldBeShutdown();

private:

	void _init(std::string host, int port, int timeout);
	std::list<NosuchSocketConnection *> _WebSocket_Clients;
	void AddWebSocketClient(NosuchSocketConnection* conn);

	NosuchJsonListener* _json_processor;
	NosuchSocket* _listening_socket;
	std::string _htmldir;
	bool _shouldbeshutdown;
	bool _shutdowncomplete;
};

#endif
