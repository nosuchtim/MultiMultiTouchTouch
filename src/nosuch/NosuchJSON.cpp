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

#include "NosuchJSON.h"
#include "NosuchUtil.h"
#include "NosuchException.h"
#include <iostream>
#include <sstream>
#include <fstream>

std::string jsonResult(std::string r, const char *id) {
	// We assume r has already been escaped if necessary
	return NosuchSnprintf("{ \"jsonrpc\": \"2.0\", \"result\": %s, \"id\": \"%s\" }\r\n",r.c_str(),id);
}

std::string jsonDoubleResult(double r, const char *id) {
	return jsonResult(NosuchSnprintf("%f",r),id);
}

std::string jsonIntResult(int r, const char *id) {
	return jsonResult(NosuchSnprintf("%d",r),id);
}

std::string jsonStringResult(std::string s, const char *id) {
	char *escaped = cJSON_escapestring(s.c_str());
	// Note - escaped now has quotes around it
	std::string r = jsonResult(escaped,id);
	cJSON_free(escaped);
	return r;
}

std::string jsonOK(const char *id) {
	return jsonIntResult(0,id);
}

std::string jsonError(int code, std::string e, const char* id) {

	char *escaped = cJSON_escapestring(e.c_str());
	// Note - escaped now has quotes around it
	std::string r = NosuchSnprintf("{ \"jsonrpc\": \"2.0\", \"error\": {\"code\": %d, \"message\": %s }, \"id\":\"%s\" }\r\n",code,escaped,id);
	cJSON_free(escaped);
	return r;
}

std::string jsonMethError(std::string e, const char *id) {
	return jsonError(-32602, e,id);
}

static cJSON* jsonGet(cJSON *j, std::string nm, int jtype) {
	NosuchAssert(j);
	cJSON *c = cJSON_GetObjectItem(j,nm.c_str());
	if ( c && c->type == jtype ) {
		return c;
	} else {
		return NULL;
	}
}

cJSON* jsonGetString(cJSON *j,std::string nm) {
	return jsonGet(j,nm,cJSON_String);
}

cJSON* jsonGetNumber(cJSON *j,std::string nm) {
	return jsonGet(j,nm,cJSON_Number);
}

cJSON* jsonGetObject(cJSON *j,std::string nm) {
	return jsonGet(j,nm,cJSON_Object);
}

cJSON* jsonGetArray(cJSON *j,std::string nm) {
	return jsonGet(j,nm,cJSON_Array);
}

std::string jsonNeedString(cJSON *j,std::string nm, std::string dflt) {
	if ( j == NULL ) {
		return dflt;
	}
	cJSON *c = cJSON_GetObjectItem(j,nm.c_str());
	if ( ! c ) {
		if ( dflt == DFLT_STR_THROW_EXCEPTION ) {
			throw NosuchException("Missing '%s' value in JSON",nm.c_str());
		}
		return dflt;
	}
	if ( c->type != cJSON_String ) {
		throw NosuchException("Unexpected type for %s value, expecting string",nm.c_str());
	}
	return c->valuestring;
}

int jsonNeedInt(cJSON *j,std::string nm, int dflt) {
	if ( j == NULL ) {
		return dflt;
	}
	cJSON *c = cJSON_GetObjectItem(j,nm.c_str());
	if ( ! c ) {
		if ( dflt == DFLT_INT_THROW_EXCEPTION ) {
			throw NosuchException("Missing '%s' value in JSON",nm.c_str());
		}
		return dflt;
	}
	if ( c->type != cJSON_Number ) {
		throw NosuchException("Unexpected type for '%s' value, expecting number",nm.c_str());
	}
	return c->valueint;
}

bool jsonNeedBool(cJSON *j,std::string nm, int dflt) {
	if ( j == NULL ) {
		return dflt != 0;
	}
	cJSON *c = cJSON_GetObjectItem(j,nm.c_str());
	if ( ! c ) {
		if ( dflt == DFLT_BOOL_THROW_EXCEPTION ) {
			throw NosuchException("Missing '%s' value in JSON",nm.c_str());
		}
		if ( dflt < 0 || dflt > 1 ) {
			throw NosuchException("Bad '%s' value in JSON",nm.c_str());
		}
		return dflt != 0;
	}
	if ( c->type != cJSON_Number ) {
		throw NosuchException("Unexpected type for '%s' value, expecting number",nm.c_str());
	}
	return c->valueint != 0;
}

double jsonNeedDouble(cJSON *j,std::string nm, double dflt) {
	if ( j == NULL ) {
		return dflt;
	}
	cJSON *c = cJSON_GetObjectItem(j,nm.c_str());
	if ( ! c ) {
		if ( dflt == DFLT_DOUBLE_THROW_EXCEPTION ) {
			throw NosuchException("Missing '%s' value in JSON",nm.c_str());
		}
		return dflt;
	}
	if ( c->type != cJSON_Number ) {
		throw NosuchException("Unexpected type for %s value, expecting double",nm.c_str());
	}
	return (double)(c->valuedouble);
}

std::string methodNeedString(std::string meth,cJSON *j,std::string nm) {
	NosuchAssert(j);
	cJSON *c = cJSON_GetObjectItem(j,nm.c_str());
	if ( ! c ) {
		throw NosuchException("Missing %s argument on %s method",nm.c_str(),meth.c_str());
	}
	if ( c->type != cJSON_String ) {
		throw NosuchException("Unexpected type for %s argument to %s method, expecting string",nm.c_str(),meth.c_str());
	}
	return c->valuestring;
}


int methodNeedInt(std::string meth,cJSON *j,std::string nm) {
	NosuchAssert(j);
	cJSON *c = cJSON_GetObjectItem(j,nm.c_str());
	if ( ! c ) {
		throw NosuchException("Missing %s argument on %s method",nm.c_str(),meth.c_str());
	}
	if ( c->type != cJSON_Number ) {
		throw NosuchException("Unexpected type for %s argument to %s method, expecting number",nm.c_str(),meth.c_str());
	}
	return c->valueint;
}

double methodNeedDouble(std::string meth,cJSON *j,std::string nm) {
	NosuchAssert(j);
	cJSON *c = cJSON_GetObjectItem(j,nm.c_str());
	if ( ! c ) {
		throw NosuchException("Missing %s argument on %s method",nm.c_str(),meth.c_str());
	}
	if ( c->type != cJSON_Number ) {
		throw NosuchException("Unexpected type for %s argument to %s method, expecting double",nm.c_str(),meth.c_str());
	}
	return (double)(c->valuedouble);
}

void methodNeedParams(std::string meth, cJSON* j) {
	if(j==NULL) {
		throw NosuchException("No parameters on %s method?",meth.c_str());
	}
}

cJSON*
jsonReadFile(std::string fname, std::string& err)
{
	std::ifstream f;
	err = "";

	f.open(fname.c_str());
	if ( ! f.good() ) {
		err = NosuchSnprintf("No config file: %s\n",fname.c_str());
		return NULL;
	}
	DEBUGPRINT(("Loading config=%s\n",fname.c_str()));
	std::string line;
	std::string jstr;
	while ( getline(f,line) ) {
		// Delete anything after a # (i.e. comments)
		std::string::size_type pound = line.find("#");
		if ( pound != line.npos ) {
			line = line.substr(0,pound);
		}
		if ( line.find_last_not_of(" \t\n") == line.npos ) {
			DEBUGPRINT1(("Ignoring blank/comment line=%s\n",line.c_str()));
			continue;
		}
		jstr += line;
	}
	f.close();
	cJSON* json = cJSON_Parse(jstr.c_str());
	if ( ! json ) {
		err = NosuchSnprintf("Unable to parse json for config!?  json= %s\n",jstr.c_str());
	}
	return json;
}

bool
jsonWriteFile(std::string fname, cJSON* json, std::string& err) {
	DEBUGPRINT(("jsonWriteFile fname=%s",fname.c_str()));
	std::ofstream f;
	f.open(fname.c_str(),std::ios::trunc);
	if ( ! f.is_open() ) {
		err = NosuchSnprintf("Unable to open file - %s",fname.c_str());
		return false;
	}
	char *s = cJSON_Print(json);
	f << s;
	f << "\n";
	f.close();
	cJSON_free(s);
	return true;
}

void
jsonFree(cJSON* j)
{
	cJSON_free(j);
}