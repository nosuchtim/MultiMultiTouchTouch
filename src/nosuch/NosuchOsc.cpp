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


#include "NosuchException.h"
#include "NosuchOsc.h"

std::string
OscMessageString(const osc::ReceivedMessage& m)
{
    const char *addr = m.AddressPattern();
    const char *types = m.TypeTags();
	if ( types == NULL ) {
		types = "";
	}

	std::string result = NosuchSnprintf("{ \"address\":\"%s\", \"args\":[",addr);
    osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
	std::string sep = "";
    for ( const char *p=types; *p!='\0'; p++ ) {
		result += sep;
        switch (*p) {
        case 'i':
			result += NosuchSnprintf("%d",(arg++)->AsInt32());
            break;
        case 'f':
			result += NosuchSnprintf("%lf",(arg++)->AsFloat());
            break;
        case 's':
			result += NosuchSnprintf("\"%s\"",(arg++)->AsString());
            break;
        case 'b':
            arg++;
			result += NosuchSnprintf("\"blob?\"");
            break;
        }
		sep = ", ";
    }
	result += "] }";
	return result;
}

void
DebugOscMessage(std::string prefix, const osc::ReceivedMessage& m)
{
	DEBUGPRINT(("%s: %s ",prefix==""?"OSC message":prefix.c_str(),OscMessageString(m).c_str()));
}

int
ArgAsInt32(const osc::ReceivedMessage& m, unsigned int n)
{
    osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
	const char *types = m.TypeTags();
	if ( n >= strlen(types) )  {
		DebugOscMessage("ArgAsInt32 ",m);
		throw NosuchException("Attempt to get argument n=%d, but not that many arguments on addr=%s\n",n,m.AddressPattern());
	}
	if ( types[n] != 'i' ) {
		DebugOscMessage("ArgAsInt32 ",m);
		throw NosuchException("Expected argument n=%d to be an int(i), but it is (%c)\n",n,types[n]);
	}
	for ( unsigned i=0; i<n; i++ )
		arg++;
    return arg->AsInt32();
}

float
ArgAsFloat(const osc::ReceivedMessage& m, unsigned int n)
{
    osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
	const char *types = m.TypeTags();
	if ( n >= strlen(types) )  {
		DebugOscMessage("ArgAsFloat ",m);
		throw NosuchException("Attempt to get argument n=%d, but not that many arguments on addr=%s\n",n,m.AddressPattern());
	}
	if ( types[n] != 'f' ) {
		DebugOscMessage("ArgAsFloat ",m);
		throw NosuchException("Expected argument n=%d to be a double(f), but it is (%c)\n",n,types[n]);
	}
	for ( unsigned i=0; i<n; i++ )
		arg++;
    return arg->AsFloat();
}

std::string
ArgAsString(const osc::ReceivedMessage& m, unsigned n)
{
    osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
	const char *types = m.TypeTags();
	if ( n < 0 || n >= strlen(types) )  {
		DebugOscMessage("ArgAsString ",m);
		throw NosuchException("Attempt to get argument n=%d, but not that many arguments on addr=%s\n",n,m.AddressPattern());
	}
	if ( types[n] != 's' ) {
		DebugOscMessage("ArgAsString ",m);
		throw NosuchException("Expected argument n=%d to be a string(s), but it is (%c)\n",n,types[n]);
	}
	for ( unsigned i=0; i<n; i++ )
		arg++;
	return std::string(arg->AsString());
}
