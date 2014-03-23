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

#ifndef _NOSUCH_JSON_H
#define _NOSUCH_JSON_H

#include "NosuchDebug.h"
#include "cJSON.h"
#include <string>

std::string jsonOK(const char *id);
std::string jsonResult(std::string r, const char *id);
std::string jsonDoubleResult(double r, const char *id);
std::string jsonIntResult(int r, const char *id);
std::string jsonStringResult(std::string r, const char *id);
std::string jsonMethError(std::string e, const char *id);
std::string jsonError(int code, std::string e, const char* id);

#define DFLT_STR_THROW_EXCEPTION "!THROW_EXCEPTION!"
#define DFLT_INT_THROW_EXCEPTION INT_MIN
#define DFLT_DOUBLE_THROW_EXCEPTION DBL_MIN
#define DFLT_BOOL_THROW_EXCEPTION 2

cJSON* jsonGetNumber(cJSON*j, std::string nm);
cJSON* jsonGetString(cJSON*j, std::string nm);
cJSON* jsonGetObject(cJSON*j, std::string nm);
cJSON* jsonGetArray(cJSON*j, std::string nm);

void jsonFree(cJSON* j);

std::string methodNeedString(std::string meth,cJSON *params,std::string nm);
int methodNeedInt(std::string meth,cJSON *params,std::string nm);
double methodNeedDouble(std::string meth,cJSON *params,std::string nm);
void methodNeedParams(std::string meth, cJSON* params);

std::string jsonNeedString(cJSON *params,std::string nm,std::string dflt=DFLT_STR_THROW_EXCEPTION);
int jsonNeedInt(cJSON *params,std::string nm,int dflt=DFLT_INT_THROW_EXCEPTION);
double jsonNeedDouble(cJSON *params,std::string nm,double dflt=DFLT_DOUBLE_THROW_EXCEPTION);
bool jsonNeedBool(cJSON *params,std::string nm,int dflt=DFLT_BOOL_THROW_EXCEPTION);

cJSON* jsonReadFile(std::string fname, std::string& err);
bool jsonWriteFile(std::string fname, cJSON* j, std::string& err);

class NosuchJsonListener {
public:
	virtual std::string processJson(std::string meth, cJSON *params, const char *id) {
		DEBUGPRINT(("HEY!  processJson in NosuchJsonProcess called!?"));
		return jsonError(-32000,"HEY, processJson in NosuchJsonProcess called?",id);
	}
};


#endif