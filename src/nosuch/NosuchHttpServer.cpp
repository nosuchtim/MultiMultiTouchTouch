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

#include <stdint.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <list>
#include <WinSock2.h>
#include "sha1.h"

#include "NosuchUtil.h"
#include "NosuchJson.h"
#include "NosuchDaemon.h"
#include "NosuchSocket.h"
#include "NosuchHttpServer.h"

NosuchHttpServer::NosuchHttpServer(NosuchJsonListener* jproc, int port, std::string htmldir, int timeout, int idletime)
{
    _listening_socket = new NosuchSocket();
	_json_processor = jproc;
	_htmldir = htmldir;
	// Someday we'll probably need the ability to specify a host,
	// but it's more foolproof to listen on all IP addresses
	// of the local host, so we set h=0
	DWORD h = 0;
	_shouldbeshutdown = false;
	_shutdowncomplete = false;
    _listening_socket->Listen(h, port, timeout, idletime);
	DEBUGPRINT(("Listening for HTTP on TCP port %d\n",port));
}

NosuchHttpServer::~NosuchHttpServer() {
	DEBUGPRINT(("~NosuchHttpServer called, deleting listening socket=%lx",(long)_listening_socket));
	delete _listening_socket;
	_listening_socket = NULL;
}

void NosuchHttpServer::Shutdown() {
	DEBUGPRINT(("NosuchHttpServer::Shutdown called, closing listening_socket"));
	_listening_socket->Close();
	DEBUGPRINT(("NosuchHttpServer::Shutdown is NOT deleting _listening_socket = %ld",(long)_listening_socket));
	// delete _listening_socket;
	// _listening_socket = NULL;
	_shutdowncomplete = true;
}

bool NosuchHttpServer::IsShutdownComplete() {
	return _shutdowncomplete;
}

bool NosuchHttpServer::ShouldBeShutdown() {
	return _shouldbeshutdown;
}

void
NosuchHttpServer::SetShouldBeShutdown(bool b) {
	_shouldbeshutdown = b;
	_shutdowncomplete = false;
}

bool
get_name_value(std::string line, std::string& name, std::string& value) {
	size_t i;
	if ( (i=line.find(":")) == line.npos ) {
		return false;
	}
	name = line.substr(0,i);
	value = line.substr(i+1);
	return true;
}

void
NosuchSocketConnection::_grab_request(int req, std::string& line) {
	_request_type = req;
	_url = line.substr(req==REQUEST_GET?4:5); // 4 is strlen("GET")+1, 5 is strlen("POST")+1
	size_t k;
	if ( (k=_url.find(" ")) != _url.npos ) {
		_url = _url.substr(0,k);
	}
}

bool
NosuchSocketConnection::CollectHttpRequest(const char *p) {
	_buff_sofar += std::string(p);

	if ( _collecting_post_data ) {
		bool done = CollectPostData(_buff_sofar);
		_buff_sofar = "";
		return done;  // true if we've collected
	}

	// Keep pulling off lines...
	size_t i;
	while ( (i=_buff_sofar.find("\n")) != _buff_sofar.npos ) {
		std::string line = _buff_sofar.substr(0,i+1);  // line includes the newline
		_buff_sofar = _buff_sofar.substr(i+1);
		// ... until you get to the end of the headers
		if ( CollectHttpHeader(line) ) {
			if ( _collecting_post_data ) {
				bool done = CollectPostData(_buff_sofar);
				_buff_sofar = "";
				return done;
			}
			return true;
		}
	}
	return false;
}

// This method returns true when POST data has been completely collected
bool
NosuchSocketConnection::CollectPostData(std::string data) {
		_data += data;
		DEBUGPRINT1(("Collected data source=%s, _data (_content_leng=%d dataleng=%d) is now=((%s))\n",_source.c_str(),_content_length,_data.length(),_data.c_str()));
		const char *dd = _data.c_str();
		if ( *dd != '{' ) {
			DEBUGPRINT(("Hey, Data doesn't being with curly! source=%s\n",_source.c_str()));
		}
		if ( _data.length() >= _content_length ) {
			return true;
		} else {
			return false;
		}
}

// This method returns true when we've read all of the headers
bool
NosuchSocketConnection::CollectHttpHeader(std::string line) {
	if ( line == "\n" || line == "\r\n" ) {
		// blank line, end of header
		if ( _content_length > 0 ) {
			DEBUGPRINT1(("End of header, content-length=%d\n",_content_length));
		}
		if ( _request_type == REQUEST_GET ) {
			return true;
		}
		if ( _request_type != REQUEST_POST ) {
			DEBUGPRINT(("HEY!  Can't handle _request_type: %d\n",_request_type));
			return true;
		}
		if ( _content_length == 0 ) {
			DEBUGPRINT(("HEY!  No Content_Length on POST?\n"));
			return true;
		}
		DEBUGPRINT1(("Setting _collecting_post_data to true! source=%s\n",_source.c_str()));
		_collecting_post_data = true;
		_data = "";
		return true;
	}
	if ( line.find("GET ") == 0 ) {
		_grab_request(REQUEST_GET,line);
		return false;
	}
	if ( line.find("POST ") == 0 ) {
		_grab_request(REQUEST_POST,line);
		return false;
	}
	std::string name;
	std::string value;
	if ( ! get_name_value(line,name,value) ) {
		DEBUGPRINT(("BAD LINE!?  line=%s\n",line.c_str()));
		return false;
	}
	for(size_t i=0; i<name.size();++i) {
		name[i] = tolower(name[i]);
	}
	value.erase(0,value.find_first_not_of(" \n\r\t"));
	value.erase(value.find_last_not_of(" \n\r\t")+1);
	DEBUGPRINT1(("HTTP header, name=%s value=%s",name.c_str(),value.c_str()));

	if ( name == "content-length" ) {
		_content_length = atoi(value.c_str());
	}
	else if ( name == "upgrade" ) {
		if ( value == "websocket" ) {
			_is_websocket = true;
		} else {
			DEBUGPRINT(("WARNING: Unable to handle upgrade (%s) that isn't websocket!?",
				value.c_str()));
		}
	}
	else if ( name == "sec-websocket-key" ) {
		_websocket_key = value;
	}
	else if ( name == "sec-websocket-version" ) {
		int version = atoi(value.c_str());
		if ( version < 13 ) {
			DEBUGPRINT(("WARNING: WebSocket-Version is < 13 ??"));
		}
	}
	return false;
}

std::string
ip_port_source(DWORD u32_IP, USHORT u16_Port_source)
{
	struct in_addr a;
	a.S_un.S_addr = u32_IP;
	return NosuchSnprintf("%d@%s",u16_Port_source,inet_ntoa(a));
}

void
NosuchHttpServer::Check()
{
    NosuchSocketMemory* recvMem = NULL;
    SOCKET  h_Socket;
	NosuchSocketConnection *h_connection;
    DWORD u32_Event, u32_IP, u32_Read, u32_Sent;
	USHORT u16_Port_source;
	int checkcount = 0;
	bool keepgoing = true;

	while ( checkcount < 5 && keepgoing ) {
		checkcount++;
		keepgoing = false;
		if ( recvMem ) {
			DEBUGPRINT1(("Before ProcessEvents, recvMem=%ld",(long)recvMem));
		}
	    DWORD u32_Err = _listening_socket->ProcessEvents(&u32_Event, &u32_IP,
			&u16_Port_source, &h_Socket, &h_connection,
			&recvMem, &u32_Read,  &u32_Sent);
		if ( recvMem ) {
			DEBUGPRINT1(("After ProcessEvents, recvMem=%ld  err=%d event=%d length=%d",(long)recvMem, u32_Err, u32_Event, recvMem->GetLength()));
		}

	    if (u32_Err == ERROR_TIMEOUT) {
	        return;
		}

	    if (u32_Event) // ATTENTION: u32_Event may be == 0 -> do nothing.
	    {
	        if (u32_Event & FD_ACCEPT ) {
	            DEBUGPRINT1(("GOT ACCEPT in http_check! socket=%ld\n", h_Socket));
			}
	        if (u32_Event & FD_CLOSE ) {
	            DEBUGPRINT1(("GOT CLOSE in http_check! socket=%ld\n", h_Socket));
				CloseWebSocket(h_connection);
			}
	        if (u32_Event & FD_CONNECT) {
	            DEBUGPRINT1(("GOT CONNECT in http_check! socket=%ld\n",h_Socket));
	        }
	        if (u32_Event & FD_WRITE) {
	            DEBUGPRINT1(("GOT WRITE in http_check! socket=%ld\n",h_Socket));
	        }
	        if (u32_Event & FD_READ && recvMem) // recvMem may be NULL if an error occurred!!
	        {
	            char z[1] = "";
	            recvMem->Append(z,1);
	            char *p = recvMem->GetBuffer();
				std::string source = ip_port_source(u32_IP,u16_Port_source);
	            DEBUGPRINT1(("recvMem socket=%ld source=%s GetBuffer=%ld\n",
					h_Socket,source.c_str(),(long)p));
	            DEBUGPRINT1(("   recvMem p=((%s))\n",p));

				if ( h_connection ) {
					bool finished = h_connection->CollectHttpRequest(p);

					if ( h_connection->parent != _listening_socket ) {
						DEBUGPRINT(("HEY!  _h_connection->parent != _listening_socket!?"));
					}

					recvMem->DeleteLeft(recvMem->GetLength());

					if ( finished ) {

						if ( h_connection->_is_websocket ) {
							if ( h_connection->h_Socket != h_Socket ) {
								DEBUGPRINT(("HEY!! NosuchSocketConnection->h_Socket != h_Socket!?"));
							}
							InitializeWebSocket(h_connection);
						} else {
							RespondToGetOrPost(h_connection);
							_listening_socket->DisconnectClient(h_Socket);
							DEBUGPRINT1(("DELETING connection source=%s\n",
								h_connection->_source.c_str()));
						}
					} else {
						DEBUGPRINT1(("HTTP request unfinished, not closing socket yet.\n"));
					}
				    recvMem = NULL;
				}
	        }
	    }

	    if (u32_Err)
	    {
	        // mi_Socket.Close() has been called -> don't print this error message
	        if (u32_Err == WSAENOTCONN) {
	            return;
			}

	        // An error normally means that the socket has a problem -> abort the loop.
	        // A few errors should not abort the processing:
	        if (u32_Err != WSAECONNABORTED && // e.g. after the other side was killed in TaskManager
	                u32_Err != WSAECONNRESET   && // Connection reset by peer.
	                u32_Err != WSAECONNREFUSED && // FD_ACCEPT with already 62 clients connected
	                u32_Err != WSAESHUTDOWN) {      // Sending data to a socket just in the short timespan
	            return;                        //   between shutdown() and closesocket()
			}
	    }
	}
	return;
}

static std::string httpheader(std::string ctype)
{
	std::string h = "HTTP/1.1 200 OK\r\nContent-Type: "+ctype+"; charset=UTF-8\r\n";
	h += "Access-Control-Allow-Origin: *\r\n";
	h += "Access-Control-Allow-Methods: GET, POST\r\n";
	h += "Access-Control-Allow-Headers: X-Requested-With\r\n";
	h += "\r\n";
	return h;
}

static void makeresult(std::string ctype, std::string data, char*& memblock, size_t& memsize) {
	std::string header = httpheader(ctype);
	size_t headersize = header.size();
	size_t datasize = data.size();
	memsize = headersize + datasize;
	memblock = new char[memsize];
	memcpy(memblock,header.c_str(),headersize);
	memcpy(memblock+headersize,data.c_str(),datasize);
}

static void httperror(std::string msg, char*& memblock, size_t& memsize) {
	makeresult("text/html",msg,memblock,memsize);
}

void
NosuchHttpServer::RespondToGetOrPost(NosuchSocketConnection *conn) {

	std::string alldata;
	std::string get_fn;
	std::string ctype = "text/html";
	char *memblock = NULL;
	size_t memsize;

	std::string urlstr(conn->_url);
	if ( urlstr == "" || urlstr == "/" ) {
		urlstr = "/index.html";
	}
	int request_type = conn->_request_type;

	if ( request_type == REQUEST_GET ) {
		size_t dot = urlstr.rfind('.');
		if ( dot != urlstr.npos ) {
			std::string suff = urlstr.substr(dot);
			if ( suff == ".html" || suff == ".htm" ) {
				ctype = "text/html";
			} else if ( suff == ".css" ) {
				ctype = "text/css";
			} else if ( suff == ".jpg" || suff == ".jpeg" ) {
				ctype = "image/jpeg";
			} else if ( suff == ".ppm" ) {
				ctype = "image/x-portable-pixmap";
			} else if ( suff == ".json" ) {
				ctype = "application/json";
			} else if ( suff == ".js" ) {
				ctype = "text/javascript";
			}
		}
		// c->_content_type = ctype;
		std::string fn = _htmldir + urlstr;

		DEBUGPRINT(("GET request for %s, reading %s\n",urlstr.c_str(),fn.c_str()));

		// Note that it is opened at the end (ate), which we use to get the size
		std::ifstream f(fn.c_str(), std::ios::in | std::ios::binary | std::ios::ate);
		if ( ! f.is_open() ) {
			httperror("Unable to open: "+fn+"\n",memblock,memsize);
			goto sendmemblock;
		}

		std::string header = httpheader(ctype);

		size_t filesize = (size_t) f.tellg();   // see note above about ate
		size_t headersize = header.size();
		memsize = headersize+filesize;
		memblock = new char[memsize];
		memcpy(memblock,header.c_str(),headersize);
		f.seekg(0,std::ios::beg);
		f.read(memblock+headersize,memsize);
		f.close();
		goto sendmemblock;
	}

	if ( request_type == REQUEST_POST ) {
		// data is the input to the POST
		std::string dd = conn->_data;   // Hmm, when I combine this with the next statement, it doesn't work?  Odd.
		const char *pp = dd.c_str();
		const char *endpp = strchr(pp,'\0');
		bool hasnewline = ( endpp && endpp > pp && *(endpp-1) == '\n' );
		if ( *pp != '{' ) {
			DEBUGPRINT(("HEY!!! No curly!?\n"));
		}
		const char *datastr = conn->_data.c_str();
		cJSON *request = cJSON_Parse(datastr);
		if ( request ) {
			std::string ret;
			std::string i;
			cJSON *c_jsonrpc = cJSON_GetObjectItem(request,"jsonrpc");
			if ( ! c_jsonrpc ) {
				ret = error_json(-32700,"No jsonrpc value in JSON!?");
				goto getout;
			}
			cJSON *c_method = cJSON_GetObjectItem(request,"method");
			if ( ! c_method ) {
				ret = error_json(-32700,"No method value in JSON!?");
				goto getout;
			}
			cJSON *c_params = cJSON_GetObjectItem(request,"params");
			// It's okay if c_params is NULL
			cJSON *c_id = cJSON_GetObjectItem(request,"id");
			if ( ! c_id ) {
				ret = error_json(-32700,"No id value in JSON!?");
				goto getout;
			}
			char *method = c_method->valuestring;
			if ( c_id->type == cJSON_Number ) {
				i = NosuchSnprintf("%d",c_id->valueint);
			} else if ( c_id->type == cJSON_String ) {
				i = NosuchSnprintf("%s",c_id->valuestring);
			} else {
				ret = error_json(-32700,"Bad id value in JSON!?");
				goto getout;
			}
			const char *id = i.c_str();
			ret = _json_processor->processJson(method,c_params,id);

getout:
			cJSON_Delete(request);

			DEBUGPRINT1(("POST response: %s\n",ret.c_str()));
			makeresult("application/json",ret,memblock,memsize);
		} else {
			httperror("Did not find jsonrpc input in POST!\n",memblock,memsize);
		}
	} else {
		httperror("Did not find GET or POST in header?\n",memblock,memsize);
	}

sendmemblock:
	if ( memblock ) {
		conn->parent->SendTo(conn->h_Socket, memblock, (DWORD) memsize );
		delete[] memblock;
	}
}
void
NosuchHttpServer::CloseWebSocket(NosuchSocketConnection* conn) {
	DEBUGPRINT1(("CloseWebSocket!!"));
	_WebSocket_Clients.remove(conn);
}

void
NosuchHttpServer::InitializeWebSocket(NosuchSocketConnection* conn) {
	std::string magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	std::string key_plus_magic = conn->_websocket_key + magic;

	SHA1_CTX context;
    uint8_t digest[20];

	const char *data = key_plus_magic.c_str();
	SHA1_Init(&context);
    SHA1_Update(&context, (uint8_t*)data, strlen(data));
    SHA1_Final(&context, digest);

	std::string hex = "0x";
	for ( int i=0; i<20; i++ ) {
		hex += NosuchSnprintf("%02x",digest[i]);
	}
	DEBUGPRINT1(("digest=%s",hex.c_str()));

	char *b64 = base64_encode(digest, 20);

	DEBUGPRINT1(("data=%s b64=%s",data,b64));

	std::string response = NosuchSnprintf(
		"HTTP/1.1 101 Switching Protocols\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Accept: %s\r\n\r\n" , b64);

	conn->parent->SendTo(conn->h_Socket, (char *)(response.c_str()), (DWORD) response.size());

	std::string msg = "{\"message\": \"Hello!\" }";
	WebSocketMessage(conn,msg);

	_WebSocket_Clients.push_back(conn);
}

void NosuchHttpServer::AddWebSocketClient(NosuchSocketConnection* conn) {
	_WebSocket_Clients.push_back(conn);
}

void
NosuchHttpServer::WebSocketMessage(NosuchSocketConnection *conn, std::string msg) {

	// See http://tools.ietf.org/html/rfc6455#page-27

	size_t msglen = msg.size();
	size_t maxframesz = msglen + 8;  // +8 is more than is absolutely necessary
	uint8_t *buff = (uint8_t*) malloc(maxframesz);
	size_t n = 0;
	buff[n++] = 0x81;  // top bit means it's the final frame, lower bit means it's text
	size_t leng = strlen(msg.c_str());
	if ( leng <= 125 ) {
		buff[n++] = (uint8_t)leng;
	} else if ( leng > 125 && leng < 1024*64 ) {
		buff[n++] = 126;
		buff[n++] = (leng >> 8) & 0xff;
		buff[n++] = (leng & 0xff);
	} else {
		buff[n++] = 127;
		buff[n++] = (leng >> 24) & 0xff;
		buff[n++] = (leng >> 16) & 0xff;
		buff[n++] = (leng >> 8) & 0xff;
		buff[n++] = (leng & 0xff);
	}
	strcpy((char*)(buff+n),msg.c_str());
	n += msglen;
	if ( conn->parent ) {
		DEBUGPRINT1(("Sending Websocket message, first few bytes are 0x%02x 0x%02x 0x%02x 0x%0x",buff[0],buff[1],buff[2],buff[3]));
		conn->parent->SendTo(conn->h_Socket,(char*)buff,(DWORD)n);
	} else {
		DEBUGPRINT(("Hey!  conn->parent is NULL?"));
	}
	free(buff);
}

void
NosuchHttpServer::SendAllWebSocketClients(std::string msg)
{
	// The msg needs to be valid JSON - this debug code verifies that
	// cJSON *j = cJSON_Parse(msg.c_str());
	// char *p = cJSON_Print(j);
	// free(p);

	for ( std::list<NosuchSocketConnection*>::iterator list_iter = _WebSocket_Clients.begin(); 
			    list_iter != _WebSocket_Clients.end(); list_iter++) {
		NosuchSocketConnection *conn = *list_iter;
		WebSocketMessage(conn,msg);
	}
}

