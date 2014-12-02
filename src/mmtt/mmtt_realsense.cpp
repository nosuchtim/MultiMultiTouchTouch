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

#include "NosuchDebug.h"
#include "mmtt.h"

#ifdef REALSENSE_CAMERA

using namespace std;

static std::map<int /*ctrl*/,pxcUID /*iuid*/> g_devices;

/* Checking if sensor device connect or not */

RealsenseDepthCamera::RealsenseDepthCamera(MmttServer* s, int camindex) {
	_server = s;
	_camindex = camindex;
	g_session = 0;
	g_capture = 0;
	g_device = 0;
	g_sensemanager = 0;
	g_colorimage = 0;
	g_depthimage = 0;
	g_coloriplimage = 0;
}

PXCCapture::Device::StreamProfileSet RealsenseDepthCamera::GetProfileSet() {
	PXCCapture::Device::StreamProfileSet profiles={};
	if (!g_device) {
		return profiles;
	}

	PXCCapture::DeviceInfo dinfo;
	g_device->QueryDeviceInfo(&dinfo);

    PXCCapture::StreamType st = dinfo.streams;
	if ( ! (st & PXCCapture::STREAM_TYPE_DEPTH) ) {
		DEBUGPRINT(("Streams don't include DEPTH!?"));
		return profiles;
	}
	if ( ! (st & PXCCapture::STREAM_TYPE_COLOR) ) {
		DEBUGPRINT(("Streams don't include COLOR!?"));
		return profiles;
	}

	int nprofiles;
	PXCCapture::StreamType lookfortype;
	int needfps = fps();
	int needwidth = width(); // 320;
	int needheight = height(); // 240;
	
	lookfortype =  PXCCapture::STREAM_TYPE_DEPTH;
	nprofiles = g_device->QueryStreamProfileSetNum(lookfortype);
	for (int n=0;n<nprofiles;n++) {
		PXCCapture::Device::StreamProfileSet pset={};
		g_device->QueryStreamProfileSet(lookfortype, n, &pset);

		if ( (pset[lookfortype].imageInfo.width == needwidth)
			&& (pset[lookfortype].imageInfo.height == needheight)
			&& (pset[lookfortype].frameRate.max >= needfps)
			&& (pset[lookfortype].frameRate.min >= needfps)
			) {

			profiles[lookfortype] = pset[lookfortype];
			break;
		}
	}

#define USE_COLOR
#ifdef USE_COLOR
	lookfortype =  PXCCapture::STREAM_TYPE_COLOR;
	nprofiles = g_device->QueryStreamProfileSetNum(lookfortype);
	for (int n=0;n<nprofiles;n++) {
		PXCCapture::Device::StreamProfileSet pset={};
		g_device->QueryStreamProfileSet(lookfortype, n, &pset);
		if ( (pset[lookfortype].imageInfo.width == needwidth)
			&& (pset[lookfortype].imageInfo.height == needheight)
			&& (pset[lookfortype].frameRate.max >= needfps)
			&& (pset[lookfortype].frameRate.min >= needfps)
			) {

			profiles[lookfortype] = pset[lookfortype];
			break;
		}
	}
#endif

	return profiles;
}

bool RealsenseDepthCamera::InitializeCamera()
{
	DEBUGPRINT(("InitializeCamera for RealSense"));
	g_session = PXCSession::CreateInstance();
	if ( !g_session ) {
		DEBUGPRINT(("Failed to create an SDK session for RealSense camera"));
		return false;
	}

	g_devices.clear();

	PXCSession::ImplDesc mdesc = {};
	mdesc.group = PXCSession::IMPL_GROUP_SENSOR;
	mdesc.subgroup = PXCSession::IMPL_SUBGROUP_VIDEO_CAPTURE;

	bool foundit = false;

	for (int m=0,iitem=0;;m++) {

		if (g_session->QueryImpl(&mdesc,m,&g_desc1)<PXC_STATUS_NO_ERROR) {
			break;
		}

		if (g_session->CreateImpl<PXCCapture>(&g_desc1,&g_capture)<PXC_STATUS_NO_ERROR) {
			continue;
		}

		DEBUGPRINT(("Scanning devices"));
		for (int j=0;;j++) {
	        if (g_capture->QueryDeviceInfo(j,&g_dinfo)<PXC_STATUS_NO_ERROR) {
				break;
			}

			std::string name = ws2s(g_dinfo.name);

			DEBUGPRINT(("g_dinfo.name = %s",name.c_str()));
			std::size_t b = name.find("RealSense");
			if ( b != std::string::npos ) {
				foundit = true;
				break;
			}
		}
		if ( foundit == true ) {
			break;
		}
		// g_capture->Release();
	}

	g_device = g_capture->CreateDevice(0);
	if (!g_device) {
		DEBUGPRINT(("DID NOT FIND RealSense!"));
		g_device->Release();
		g_device = 0;
		g_capture = NULL;
		return false;
	}

	if ( ! foundit ) {
		DEBUGPRINT(("DID NOT FIND RealSense!"));
		g_capture = NULL;
		return false;
	}

	g_sensemanager = g_session->CreateSenseManager();
	if ( !g_sensemanager ) {
		DEBUGPRINT(("UNABLE TO CreateSenseManager"));
		g_capture = NULL;
		return false;
	}

	DEBUGPRINT(("FOUND RealSense!"));

    /* Set device source */
    g_sensemanager->QueryCaptureManager()->FilterByDeviceInfo(&g_dinfo);

	g_profiles = GetProfileSet();

    /* Enable all selected streams in handler */
	g_handler = new MyHandler(g_sensemanager,g_profiles);

    /* Initialization */
    if (g_sensemanager->Init(g_handler)<PXC_STATUS_NO_ERROR) {\
		DEBUGPRINT(("sensemanager Init failed!"));
		g_capture = NULL;
		return false;
    }

	DEBUGPRINT(("INITIALIZED RealSense!"));

	return true;
}

void
RealsenseDepthCamera::processRawDepth(pxcU16 *depth)
{
	uint16_t *depthmm = server()->depthmm_mid;
	uint8_t *depthbytes = server()->depth_mid;
	uint8_t *threshbytes = server()->thresh_mid;

	int i = 0;

	bool filterbydepth = ! server()->val_showrawdepth.value;

	// XXX - THIS NEEDS OPTIMIZATION!

	const pxcU16 *pdepth = depth;

	int h = height();
	int w = width();
	for (int y=0; y<h; y++) {
		float backval;
		if ( server()->continuousAlign == true ) {
			backval = (float)(server()->val_depth_align.value);
		} else {
			backval = (float)(server()->val_depth_detect_top.value
				+ (server()->val_depth_detect_bottom.value - server()->val_depth_detect_top.value)
				* (float(y)/h));
		}

	  pxcU16* lastpixel = depth + (y+1)*w - 1;
	  for (int x=0; x<w; x++,i++) {
		uint16_t mm = *(lastpixel--);
		depthmm[i] = mm;
#define OUT_OF_BOUNDS 99999
		int deltamm;
		int pval = 0;
		if ( filterbydepth ) {
			if ( mm == 0 || mm < server()->val_depth_front.value || mm > backval ) {
				pval = OUT_OF_BOUNDS;
			} else if ( x < server()->val_edge_left.value || x > server()->val_edge_right.value || y < server()->val_edge_top.value || y > server()->val_edge_bottom.value ) {
				pval = OUT_OF_BOUNDS;
			} else {
				// deltamm = (int)val_depth_detect_top.value - mm;
				deltamm = (int)backval - mm;
			}
		} else {
			deltamm = (int)backval - mm;
		}
		if ( pval == OUT_OF_BOUNDS || (pval>>8) > 5 ) {
			// black
			*depthbytes++ = 0;
			*depthbytes++ = 0;
			*depthbytes++ = 0;
			threshbytes[i] = 0;
		} else {
			// white
			uint8_t mid = 255 - (deltamm/10);  // a little grey-level based on distance
			*depthbytes++ = mid;
			*depthbytes++ = mid;
			*depthbytes++ = mid;
			threshbytes[i] = 255;
		}
	  }
	}
}

void RealsenseDepthCamera::Update() {
	bool sync = true;
	pxcStatus status;
	
	status = g_sensemanager->AcquireFrame(sync);
	if ( status < PXC_STATUS_NO_ERROR ) {
		DEBUGPRINT(("AcquireFrame returned error?"));
		return;
	}
	PXCCapture::Sample *sample = (PXCCapture::Sample*)g_sensemanager->QuerySample();

	if ( sample == NULL ) {
		DEBUGPRINT(("QuerySample returned NULL?"));
		goto getout;
	}

	if ( sample->depth == NULL || sample->color == NULL ) {
		DEBUGPRINT(("Hey, depth and color weren't both present?"));
		return;
	}

	if ( g_colorimage ) {
		g_colorimage->Release();
		g_colorimage = 0;
	}

	if ( g_depthimage ) {
		g_depthimage->Release();
		g_depthimage = 0;
	}

	g_depthimage = sample->depth;
	g_depthimage->AddRef();

	g_colorimage = sample->color;
	g_colorimage->AddRef();

	PXCImage::ImageData depthidata;
	status = g_depthimage->AcquireAccess(PXCImage::ACCESS_READ,&depthidata);
	if ( status < PXC_STATUS_NO_ERROR ) {
		DEBUGPRINT(("AcquireAccess returned error?"));
	} else {
		processRawDepth((pxcU16*)depthidata.planes[0]);
	}

	if ( g_coloriplimage == NULL ) {
		// One-time
		g_coloriplimage = cvCreateImageHeader( cvSize(width(),height()), IPL_DEPTH_8U, 3 );
	}

	PXCImage::ImageData coloridata;
	status = g_colorimage->AcquireAccess(PXCImage::ACCESS_READ,PXCImage::PIXEL_FORMAT_RGB24,&coloridata);
	if ( status < PXC_STATUS_NO_ERROR ) {
		DEBUGPRINT(("AcquireAccess returned error?"));
		goto getout;
	}

	status = g_colorimage->ExportData(&coloridata);
	if ( status < PXC_STATUS_NO_ERROR ) {
		DEBUGPRINT(("Unable to ExportData from g_colorimage!?"));
		goto getout;
	}

	g_coloriplimage->origin = 1;
	// g_coloriplimage->imageData = (char*)(projecteddata.planes[0]);
	g_coloriplimage->imageData = (char*)(coloridata.planes[0]);

	g_colorimage->ReleaseAccess(&coloridata);
	g_depthimage->ReleaseAccess(&depthidata);

getout:
	g_sensemanager->ReleaseFrame();
};

IplImage*
RealsenseDepthCamera::colorimage() {
	return g_coloriplimage;
}

void RealsenseDepthCamera::Shutdown() {
	g_session->Release();
};

#endif