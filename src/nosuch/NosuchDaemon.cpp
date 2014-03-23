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

#include "NosuchUtil.h"
#include "NosuchException.h"
#include "NosuchOscInput.h"
#include "NosuchOscManager.h"
#include "NosuchHttpServer.h"
#include "NosuchDaemon.h"
#include <pthread.h>

void *network_threadfunc(void *arg)
{
	NosuchDaemon* b = (NosuchDaemon*)arg;
	return b->network_input_threadfunc(arg);
}

NosuchDaemon::NosuchDaemon(
	int osc_port,
	std::string osc_host,
	NosuchOscListener* oscproc,
	int http_port,
	std::string html_dir,
	NosuchJsonListener* jsonproc)
{
	DEBUGPRINT2(("NosuchDaemon CONSTRUCTOR!"));

	daemon_shutting_down = false;
	_httpserver = NULL;
	_oscinput = NULL;
	_listening = false;
	_network_thread_created = false;

	if ( NosuchNetworkInit() ) {
		DEBUGPRINT(("Unable to initialize networking in NosuchDaemon constructor, there will be no listeners"));
		return;
	}

	_listening = true;

	_httpserver = new NosuchHttpServer(jsonproc, http_port, html_dir, 60, 60000);

	if ( osc_port < 0 || oscproc == NULL ) {
		DEBUGPRINT(("NOT listening for OSC because oscport<0 or missing"));
		_oscinput = NULL;
	} else {
		_oscinput = new NosuchOscManager(oscproc,osc_host,osc_port);
		_oscinput->Listen();
	}

	DEBUGPRINT2(("About to use pthread_create in NosuchDaemon"));
	int err = pthread_create(&_network_thread, NULL, network_threadfunc, this);
	if (err) {
		NosuchErrorOutput("pthread_create failed!? err=%d",err);
	} else {
		_network_thread_created = true;
	}
}

NosuchDaemon::~NosuchDaemon()
{
	daemon_shutting_down = true;
	if ( _network_thread_created ) {
		// pthread_detach(_network_thread);
		pthread_join(_network_thread,NULL);
	}

	NosuchAssert(_httpserver != NULL);

	delete _httpserver;
	_httpserver = NULL;

	if ( _oscinput ) {
		_oscinput->UnListen();
		delete _oscinput;
		_oscinput = NULL;
	}
}

void
NosuchDaemon::SendAllWebSocketClients(std::string msg)
{
	if ( _httpserver ) {
		_httpserver->SendAllWebSocketClients(msg);
	}
}

void *NosuchDaemon::network_input_threadfunc(void *arg)
{
	int textcount = 0;

	int cnt=100;
	while (cnt-- > 0) {
		Sleep(1);
	}
	while (daemon_shutting_down == false ) {

		// Listening is turned off when Resolume makes a clip inactive.
		if ( ! _listening ) {
			Sleep(100);
			continue;
		}
		if ( _httpserver ) {
			if ( _httpserver->ShouldBeShutdown() ) {
				_httpserver->Shutdown();
				delete _httpserver;
				_httpserver = NULL;
			} else {
				_httpserver->Check();
			}
		}
		if ( _oscinput ) {
			if ( _oscinput->ShouldBeShutdown() ) {
				_oscinput->Shutdown();
				delete _oscinput;
				_oscinput = NULL;
			} else {
				_oscinput->Check();
			}
		}
		Sleep(1);
	}

	return NULL;
}