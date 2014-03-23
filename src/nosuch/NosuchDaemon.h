#ifndef _NOSUCH_DAEMON_H
#define _NOSUCH_DAEMON_H

#include <pthread.h>
#include <string>

class NosuchJsonListener;
class NosuchHttpServer;
class NosuchOscManager;
class NosuchOscListener;

class NosuchDaemon {
public:
	NosuchDaemon(
		int osc_port,
		std::string osc_host,
		NosuchOscListener* oscproc,
		int http_port,
		std::string html_dir,
		NosuchJsonListener* jsonproc);
	~NosuchDaemon();
	void *network_input_threadfunc(void *arg);

	void SendAllWebSocketClients(std::string msg);

private:
	bool _network_thread_created;
	bool daemon_shutting_down;
	pthread_t _network_thread;
	NosuchOscManager* _oscinput;
	NosuchHttpServer* _httpserver;
	// std::list<VizletHttpClient*> _httpclients;

	bool _listening;
};

#endif