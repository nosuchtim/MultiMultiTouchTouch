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


#include <math.h>
#include <string>
#include <sstream>
#include <intrin.h>
#include <float.h>

#include "NosuchUtil.h"
#include "NosuchOscInput.h"
#include "NosuchOscManager.h"
#include "NosuchOscTcpInput.h"
#include "NosuchOscUdpInput.h"

NosuchOscManager::NosuchOscManager(NosuchOscListener* server, std::string host, int port) {

	DEBUGPRINT2(("NosuchOscManager constructor port=%d",port));
	// _seq = -1;
	// _tcp = new NosuchOscTcpInput(host,port);
	_tcp = NULL;
	_udp = new NosuchOscUdpInput(host,port,server);
	_shouldbeshutdown = false;
	_shutdowncomplete = false;
}

NosuchOscManager::~NosuchOscManager() {
	if ( _tcp )
		delete _tcp;
	if ( _udp )
		delete _udp;
}

void
NosuchOscManager::Shutdown() {
	DEBUGPRINT(("NosuchOscManager::Shutdown called, closing listening_socket"));
	UnListen();
	_shutdowncomplete = true;
}

bool NosuchOscManager::IsShutdownComplete() {
	return _shutdowncomplete;
}

bool NosuchOscManager::ShouldBeShutdown() {
	return _shouldbeshutdown;
}

void
NosuchOscManager::SetShouldBeShutdown(bool b) {
	_shouldbeshutdown = b;
	_shutdowncomplete = false;
}

void
NosuchOscManager::Check() {
	if ( _tcp )
		_tcp->Check();
	if ( _udp )
		_udp->Check();
}

void
NosuchOscManager::UnListen() {
	if ( _tcp )
		_tcp->UnListen();
	if ( _udp )
		_udp->UnListen();
}

int
NosuchOscManager::Listen() {
	int e;
	if ( _tcp ) {
		if ( (e=_tcp->Listen()) != 0 ) {
			if ( e == WSAEADDRINUSE ) {
				NosuchErrorOutput("TCP port/address (%d/%s) is already in use?",_tcp->Port(),_tcp->Host().c_str());
			} else {
				NosuchErrorOutput("Error in _tcp->Listen = %d\n",e);
			}
			return e;
		}
	}
	if ( _udp ) {
		if ( (e=_udp->Listen()) != 0 ) {
			if ( e == WSAEADDRINUSE ) {
				NosuchErrorOutput("UDP port/address (%d/%s) is already in use?",_udp->Port(),_udp->Host().c_str());
			} else {
				NosuchErrorOutput("Error in _udp->Listen = %d\n",e);
			}
			return e;
		}
	}
	return 0;
}
