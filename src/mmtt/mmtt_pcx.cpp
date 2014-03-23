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

#ifdef PCX_CAMERA

using namespace std;

MyUtilCapture::MyUtilCapture(PXCSession *session) : UtilCapture(session) {
	device_number = -1;
}

void
MyUtilCapture::SetDeviceNumberFilter(int device_number) {
	this->device_number = device_number;
}

void MyUtilCapture::ScanProfiles(std::list<PXCCapture::VideoStream::ProfileInfo> &profiles,
			PXCImage::ImageType stype, PXCCapture::VideoStream *vstream) {

	PXCCapture::DeviceInfo dinfo;
	m_device->QueryDevice(&dinfo);
	if (device_number < 0 || dinfo.didx==device_number) {
		UtilCapture::ScanProfiles(profiles,stype,vstream);
	}
}

PcxDepthCamera::PcxDepthCamera(MmttServer* s, int camindex) {
	_server = s;
	_camindex = camindex;
}

bool PcxDepthCamera::InitializeCamera()
{
	DEBUGPRINT(("InitializeCamera for Creative"));
	pxcStatus sts = PXCSession_Create(&_session);
	if ( sts < PXC_STATUS_NO_ERROR ) {
		DEBUGPRINT(("Failed to create an SDK session for Creative camera"));
		return false;
	}

	// The ability of this code to grab multiple cameras in the same process
	// isn't currently used.  To use multiple cameras, multiple instances
	// of mmtt are used, each with a different mmtt.json config that uses
	// the "cameraindex" value to select a specific camera.
    _capture[0] = new MyUtilCapture(_session);
	_capture[0]->SetDeviceNumberFilter(_camindex);
	_capture[1] = NULL;
    
    PXCCapture::VideoStream::DataDesc request; 
    memset(&request, 0, sizeof(request));
    request.streams[0].format=PXCImage::COLOR_FORMAT_DEPTH;
    // request.streams[1].format=PXCImage::COLOR_FORMAT_DEPTH;
	for ( int c=0; c<MAX_PCX_CAMERAS; c++ ) {
		if ( _capture[c] ) {
		    sts = _capture[c]->LocateStreams (&request); 
		    if (sts<PXC_STATUS_NO_ERROR) {
		        DEBUGPRINT(("Failed to locate depth stream(s) for capture[%d]!!",c));
		        return false;
			}
			gatherStreams(_capture[c]);
		}
    }

    int i;
	_nstreams = (int)_streams.size();
	DEBUGPRINT(("_nstreams = %d",_nstreams));

	_sps = new PXCSmartSPArray(_nstreams);
	_image = new PXCSmartArray<PXCImage>(_nstreams);

    for (i=0;i<_nstreams;i++) { 
        sts=_streams[i]->ReadStreamAsync (&(*_image)[i], &(*_sps)[i]); 
        // if (sts>=PXC_STATUS_NO_ERROR) _nwindows++;
    }

	return true;
}

void PcxDepthCamera::gatherStreams(MyUtilCapture* capture) {
    for (int idx=0;;idx++) {
		DEBUGPRINT(("InitializeCamera doing queryvideostream for idx=%d",idx));
        PXCCapture::VideoStream *stream_v=capture->QueryVideoStream(idx);
        if (!stream_v) {
			DEBUGPRINT(("No stream for capture=%d idx=%d",(int)capture,idx));
			break;
		}
		DEBUGPRINT(("queryvideostream for idx=%d FOUND ONE!",idx));

        PXCCapture::VideoStream::ProfileInfo pinfo;
        stream_v->QueryProfile(&pinfo);
        WCHAR stream_name[256];
        switch (pinfo.imageInfo.format&PXCImage::IMAGE_TYPE_MASK) {
        case PXCImage::IMAGE_TYPE_COLOR: 
            swprintf_s<256>(stream_name, L"Stream#%d (Color) %dx%d", idx, pinfo.imageInfo.width, pinfo.imageInfo.height);
            break;
        case PXCImage::IMAGE_TYPE_DEPTH: 
            swprintf_s<256>(stream_name, L"Stream#%d (Depth) %dx%d", idx, pinfo.imageInfo.width, pinfo.imageInfo.height);
            break;
        }
        _streams.push_back(stream_v);
    }
}

void PcxDepthCamera::ProcessDepth() {
	int nframes = 50000;
	pxcStatus sts;

	pxcU32 sidx=0;

#ifdef DO_EXTRA_SYNC
	sts = (*_sps).SynchronizeEx(&sidx,1);  // timeout is 1 millisecond
	if ( sts == PXC_STATUS_EXEC_TIMEOUT ) {
		return;
	}
	if (sts<PXC_STATUS_NO_ERROR ) {
		DEBUGPRINT(("SynchronizeEx(sidx) returned %d",sts));
		return; // break;
	}
#endif

	// loop through all active streams as SynchronizeEx only returns the first active
    for (int j=(int)sidx;j<_nstreams;j++) {
        if (!(*_sps)[j]) {
			continue;
		}
		pxcStatus status = (*_sps)[j]->Synchronize(500);
		if ( status == PXC_STATUS_EXEC_INPROGRESS ) {
			DEBUGPRINT(("synchronize inprogress"));
			continue;
		}
		if ( status == PXC_STATUS_EXEC_TIMEOUT ) {
			continue;
		}
		if ( status != PXC_STATUS_NO_ERROR ) {
			DEBUGPRINT(("synchronize(0) error=%d",status));
			continue;
		}
	    (*_sps).ReleaseRef(j);

		PXCImage* pi = (*_image)[j];
		PXCImage::ImageInfo info;
		sts = pi->QueryInfo(&info);
		if ( sts == PXC_STATUS_NO_ERROR ) {
			PXCImage::ImageData data;
			sts = pi->TryAccess(PXCImage::ACCESS_READ,PXCImage::COLOR_FORMAT_DEPTH,&data);
			if ( sts == PXC_STATUS_NO_ERROR ) {
				pxcBYTE* dplane = data.planes[0];
				pxcBYTE* confidence = data.planes[1];
				processRawDepth((pxcU16*)dplane,(pxcU16*)confidence);
				pi->ReleaseAccess(&data);
			} else {
				DEBUGPRINT(("sts from TryAccess = %d",sts));
			}
		}
        sts=_streams[j]->ReadStreamAsync((*_image).ReleaseRef(j), &(*_sps)[j]);
        if (sts<PXC_STATUS_NO_ERROR) {
			(*_sps)[j]=0;
		}
    }
#ifdef DO_EXTRA_SYNC
    sts = (*_sps).SynchronizeEx();
	if (sts<PXC_STATUS_NO_ERROR) {
		DEBUGPRINT(("SynchronizeEx() returned %d",sts));
		return; // break;
	}
#endif
}

void
PcxDepthCamera::processRawDepth(pxcU16 *depth,pxcU16* confidence)
{
	uint16_t *depthmm = server()->depthmm_mid;
	uint8_t *depthbytes = server()->depth_mid;
	uint8_t *threshbytes = server()->thresh_mid;

	int i = 0;

	bool filterdepth = ! server()->val_showrawdepth.internal_value;

	// XXX - THIS NEEDS OPTIMIZATION!

	const pxcU16 *pdepth = depth;
	const pxcU16 *pconfidence = confidence;

	int h = height();
	int w = width();
	for (int y=0; y<h; y++) {
	  float backval = (float)(server()->val_backtop.internal_value
		  + (server()->val_backbottom.internal_value - server()->val_backtop.internal_value)
		  * (float(y)/h));

	  pxcU16* lastpixel = depth + (y+1)*w - 1;
	  pxcU16* lastconf = confidence + (y+1)*w - 1;
	  for (int x=0; x<w; x++,i++) {
		// uint16_t mm = *(pdepth++);
		uint16_t mm = *(lastpixel--);
		uint16_t conf = *(lastconf--);
		// conf value is 0-3000?
		// conf = conf / 16;

		depthmm[i] = mm;
#define OUT_OF_BOUNDS 99999
		int deltamm;
		int pval = 0;
		if ( filterdepth ) {
			if ( mm == 0 || mm < server()->val_front.internal_value || mm > backval ) {
				pval = OUT_OF_BOUNDS;
			} else if ( x < server()->val_left.internal_value || x > server()->val_right.internal_value || y < server()->val_top.internal_value || y > server()->val_bottom.internal_value ) {
				pval = OUT_OF_BOUNDS;
			} else if ( conf < server()->val_confidence.internal_value ) {
				pval = OUT_OF_BOUNDS;
			} else {
				// deltamm = (int)val_backtop.internal_value - mm;
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

void PcxDepthCamera::Update() {
	ProcessDepth();
};

void PcxDepthCamera::Shutdown() {
};

#endif