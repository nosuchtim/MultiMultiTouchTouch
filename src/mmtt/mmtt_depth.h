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

#ifndef MMTT_DEPTH_H
#define MMTT_DEPTH_H

#include <string>

class MmttServer;

class DepthCamera {
public:
	static DepthCamera* makeDepthCamera(MmttServer* s, std::string camtype, int camnum, int camwidth, int camheight);

	MmttServer* server() { return _server; }

	virtual const int width() = 0;
	virtual const int height() = 0;
	virtual const int default_depth_detect_top() = 0;
	virtual const int default_depth_detect_bottom() = 0;
	virtual bool InitializeCamera() = 0;
	virtual bool Update(uint16_t *depthmm, uint8_t *depthbytes, uint8_t *threshbytes) = 0;
	virtual void Shutdown() = 0;
	virtual std::string camtype() = 0;

	// These don't have to defined by all cameras
	virtual bool Tilt(int degrees) { return false; };
	virtual IplImage* colorimage() { return NULL; };

protected:
	MmttServer* _server;
	std::string _camtype;
	int _camindex;
};

#endif
