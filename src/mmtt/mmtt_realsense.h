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

#ifndef MMTT_REALSENSE_H
#define MMTT_REALSENSE_H

#include <vector>
#include <map>
#include <iostream>

// #define USE_COLOR
// #define GENERATE_MAPPED_DEPTH
// #define USE_MAPPED_DEPTH

#include "pxcprojection.h"
#include "pxcsensemanager.h"
#include "pxccapture.h"

#include "ip/NetworkingUtils.h"

#include "OscSender.h"
#include "OscMessage.h"
#include "BlobResult.h"
#include "blob.h"

#include "NosuchHttpServer.h"
#include "NosuchException.h"

#include "mmtt_depth.h"

class UT_SharedMem;
class MMTT_SharedMemHeader;
class TOP_SharedMemHeader;

#define MAX_SENZ3D_CAMERAS 2

#define _USE_MATH_DEFINES // To get definition of M_PI
#include <math.h>

class MyHandler: public PXCSenseManager::Handler {
public:
	virtual pxcStatus PXCAPI OnConnect(PXCCapture::Device *device, pxcBool connected) {
		DEBUGPRINT((connected?"Device Connected":"Device Disconnected"));
		PXCCapture::DeviceInfo dinfo;
	    device->QueryDeviceInfo(&dinfo);
        if (connected) {
            if (sm) {

                /* Enable all selected streams */
                PXCCapture::Device::StreamProfileSet _profiles={};
                device->QueryStreamProfileSet(PXCCapture::STREAM_TYPE_COLOR|PXCCapture::STREAM_TYPE_DEPTH|PXCCapture::STREAM_TYPE_IR, 0, &_profiles);
	            for (PXCCapture::StreamType st=PXCCapture::STREAM_TYPE_COLOR;st!=PXCCapture::STREAM_TYPE_ANY;st++) {
		            PXCCapture::Device::StreamProfile &profile=profiles[st];
		            if (!profile.imageInfo.format) {
						continue;
					}
                    if (device->QueryStreamProfileSetNum(st)==1) {
						profile = _profiles[st];
					}
		            sm->EnableStream(st,profile.imageInfo.width, profile.imageInfo.height, profile.frameRate.max);
	            }
	            sm->QueryCaptureManager()->FilterByStreamProfiles(&profiles);
            }
        }
		return PXC_STATUS_NO_ERROR;
	}

	MyHandler(PXCSenseManager* sm,PXCCapture::Device::StreamProfileSet profiles) {
		this->sm=sm;
		this->profiles=profiles;
	}

protected:
    PXCSenseManager* sm;
	PXCCapture::Device::StreamProfileSet profiles;
};

class RealsenseDepthCamera : public DepthCamera {
public:
	RealsenseDepthCamera(MmttServer* s, int num);
	~RealsenseDepthCamera();
	const int width() { return 640; }
	const int height() { return 480; }
	const int fps() { return 30; }
	const int default_depth_detect_top() { return 700; };
	const int default_depth_detect_bottom() { return 700; };
	bool InitializeCamera();
	void Update();
	void Shutdown();
	std::string camtype() {
		return NosuchSnprintf("Realsense camera");
	}
#ifdef USE_COLOR
	IplImage* colorimage();
#endif
	IplImage* depthimage();

private:

	PXCSession* g_session;
	PXCCapture *g_capture;
	PXCCapture::Device *g_device;
	PXCCapture::DeviceInfo g_dinfo;
	PXCSession::ImplDesc g_desc1;
	PXCSenseManager *g_sensemanager;
	PXCCapture::Device::StreamProfileSet g_profiles;
	MyHandler* g_handler;
	PXCCapture::Device::StreamProfileSet GetProfileSet();

	PXCImage* g_depthimage;
#ifdef USE_COLOR
	PXCImage* g_colorimage;
#endif

#ifdef GENERATE_MAPPED_DEPTH
	PXCProjection *g_projection;
	PXCImage* g_projected;
#endif

	IplImage* g_coloriplimage;

	int _nactive;
	int _nstreams;

	void processRawDepth(pxcU16* depth);
};

#endif
