#ifndef _NSOSCUDP
#define _NSOSCUDP

#include "NosuchOscInput.h"
#include "winsock.h"

class NosuchOscUdpInput : public NosuchOscInput {

public:
	NosuchOscUdpInput(std::string host, int port, NosuchOscListener* processor);
	virtual ~NosuchOscUdpInput();
	int Listen();
	void Check();
	void UnListen();
	std::string Host() { return _myhost; }
	int Port() { return _myport; }

private:
	SOCKET _s;
	int _myport;
	std::string _myhost;
	NosuchOscListener* _processor;
};

#endif