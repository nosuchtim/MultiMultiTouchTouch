/*
 *  Created by Tim Thompson on 1/31/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef MANIFOLD_OSC_INPUT
#define MANIFOLD_OSC_INPUT

#include "NosuchOsc.h"

class NosuchOscTcpInput;
class NosuchOscUdpInput;
class Vizlet;

class NosuchOscManager {

public:
	NosuchOscManager(NosuchOscListener* server, std::string host, int port);
	~NosuchOscManager();
	void Check();
	int Listen();
	void UnListen();
	void processOsc(const char *source, const osc::ReceivedMessage& m);
	void Shutdown();
	void SetShutdownComplete(bool);
	bool IsShutdownComplete();
	void SetShouldBeShutdown(bool);
	bool ShouldBeShutdown();

private:
	NosuchOscTcpInput* _tcp;
	NosuchOscUdpInput* _udp;
	bool _shouldbeshutdown;
	bool _shutdowncomplete;
};

#endif
