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

#ifndef MMTT_PCX_H
#define MMTT_PCX_H

#include <vector>
#include <map>
#include <iostream>

#include "ip/NetworkingUtils.h"

#include "OscSender.h"
#include "OscMessage.h"
#include "BlobResult.h"
#include "blob.h"

#include "NosuchHttpServer.h"
#include "NosuchException.h"

class UT_SharedMem;
class MMTT_SharedMemHeader;
class TOP_SharedMemHeader;

#include "pxcsession.h"
#include "pxcsmartptr.h"
#include "pxccapture.h"
#include "util_render.h"
#include "util_capture_file.h"
#include "util_cmdline.h"

#define MAX_PCX_CAMERAS 2

#define _USE_MATH_DEFINES // To get definition of M_PI
#include <math.h>

class MyUtilCapture: public UtilCapture {
public:
	MyUtilCapture(PXCSession *session);
	void SetDeviceNumberFilter(int device_number);
protected:
	int device_number;
	virtual void ScanProfiles(std::list<PXCCapture::VideoStream::ProfileInfo> &profiles,
			PXCImage::ImageType stype, PXCCapture::VideoStream *vstream);
};

class PcxDepthCamera : public DepthCamera {
public:
	PcxDepthCamera(MmttServer* s, int num);
	~PcxDepthCamera();
	const int width() { return 320; }
	const int height() { return 240; }
	const int default_backtop() { return 700; };
	const int default_backbottom() { return 700; };
	bool InitializeCamera();
	void Update();
	void Shutdown();
	std::string camtype() {
		return NosuchSnprintf("Creative Senz3D (index: %d)",_camindex);
	}

private:

	PXCSession* _session;
	std::vector<PXCCapture::VideoStream*> _streams;
    PXCSmartSPArray* _sps;
    PXCSmartArray<PXCImage>* _image;
	int _nactive;
	int _nstreams;
	MyUtilCapture* _capture[MAX_PCX_CAMERAS];

	void ProcessDepth();
	void processRawDepth(pxcU16* depth, pxcU16* confidence);
	void gatherStreams(MyUtilCapture* capture);
};

#endif
