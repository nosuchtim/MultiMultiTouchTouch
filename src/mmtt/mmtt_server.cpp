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

#include <winsock2.h>
#pragma comment(lib, "ws2_32")

#include <list>

#include "stdafx.h"
#include "stdint.h"
#include "sha1.h"

#include <GL/gl.h>
#include <GL/glu.h>
 
#include <pthread.h>
#include <opencv/cv.h>
#include <opencv/highgui.h>

#include "NosuchDebug.h"
#include "NosuchSocket.h"
#include "NosuchHttpServer.h"
#include "mmsystem.h"
#include "porttime.h"
#include "mmtt.h"

#include "BlobResult.h"

#include "OscSender.h"
#include "OscMessage.h"

#include <iostream>
#include <sstream>
#include <fstream>

#include "UT_SharedMem.h"
#include "UT_Mutex.h"
#include "mmtt_sharedmem.h"

#include "NuiApi.h"

MmttServer* ThisServer;

using namespace std;

extern "C" {

int MaskDilateAmount = 2;

typedef struct RGBcolor {
	int r;
	int g;
	int b;
} RGBcolor;

static RGBcolor region2color[] = {
	{ 0,    0,  0 }, //region = 0   is this used?
	{ 255,  0,  0 }, //region = 1   background
	{ 0  ,255,  0 }, //region = 2
	{ 0  ,  0,255 }, //region = 3
	{ 255,255,  0 }, //region = 4
	{ 0  ,255,255 }, //region = 5
	{ 255,  0,255 }, //region = 6
	{ 255,128,  0 }, //region = 7
	{ 128,255,  0 }, //region = 8
	{ 128,  0,255 }, //region = 9
	{ 255,  0,128 }, //region = 10
	{ 0  ,255,128 }, //region = 11
	{ 0  ,128,255 }, //region = 12
	{ 255,128,128 }, //region = 13
	{ 128,255,128 }, //region = 14
	{ 128,128,255 }, //region = 15
	{ 255,255,128 }, //region = 16
	{ 64, 128,128 }, //region = 17
	{ 128, 64,128 }, //region = 18
	{ 128,128,64  }, //region = 19
	{ 64,  64,128 }, //region = 20
	{ 128, 64,64  }, //region = 21
	{ 255,255,255 }, //region = mask
};

#define MAX_REGION_ID 22
#define MASK_REGION_ID 22

static CvScalar region2cvscalar[MAX_REGION_ID+1];

};  // extern "C"

static float raw_depth_to_meters(int raw_depth)
{
	if ( raw_depth < 2047 ) {
		return 1.0f / (raw_depth * -0.0030711016f + 3.3309495161f);
	}
	return 0.0f;
}

MmttServer::MmttServer(std::string configpath)
{
	ThisServer = this;  // should try to remove this eventually 

	NosuchErrorPopup = MmttServer::ErrorPopup;

	_status = "Uninitialized";
	_configpath = configpath;
	_regionsfilled = false;
	_regionsDefinedByPatch = false;
	_python_disabled = false;

	registrationState = 0;
	continuousAlign = false;

	// These are the defaults.  If any explicit clients are
	// given on the command-line, these are ignored.
	_Tuio1_2_OscClientList = "";
	_Tuio1_25_OscClientList = ""; // "127.0.0.1:3333;192.168.1.132:3333";
	_Tuio2_OscClientList = "";  // "127.0.0.1:6666";

	_tuio_last_sent = 0;
	_tuio_fseq = 1;

	_newblobresult = NULL;
	_oldblobresult = NULL;
	shutting_down = FALSE;
	mFirstDraw = TRUE;
	_httpserver = NULL;
	lastFpsTime = 0.0;
	doBlob = TRUE;
	doBW = TRUE;
	doSmooth = FALSE;
	autoDepth = FALSE;
	currentRegionValue = 1;
	do_setnextregion = FALSE;

	json_mutex = PTHREAD_MUTEX_INITIALIZER;
	json_cond = PTHREAD_COND_INITIALIZER;
	json_pending = FALSE;

	init_regular_values();

	std::string err = LoadGlobalDefaults();
	if ( err != "" ) {
		DEBUGPRINT((err.c_str()));
		FatalAppExit( NULL, s2ws(err).c_str());
	}

	// The config may have decided to disable popup errors, since they
	// cause the program to stop until the popup is dismissed.
	if ( ! _do_errorpopup ) {
		NosuchErrorPopup = NULL;
	}

	camera = DepthCamera::makeDepthCamera(this,_cameraType,_cameraIndex);
	if ( ! camera ) {
		std::string msg = NosuchSnprintf("Unrecognized depth camera type '%s'",_cameraType.c_str());
		DEBUGPRINT((msg.c_str()));
		FatalAppExit( NULL, s2ws(msg).c_str());
	}

	init_camera_values();

	_camWidth = camera->width();
	_camHeight = camera->height();
	_camBytesPerPixel = 3;
	_fpscount = 0;
	_framenum = 0;

	depth_mid = (uint8_t*)malloc(_camWidth*_camHeight*_camBytesPerPixel);
	depth_front = (uint8_t*)malloc(_camWidth*_camHeight*_camBytesPerPixel);

	depthmm_mid = (uint16_t*)malloc(_camWidth*_camHeight*sizeof(uint16_t));
	depthmm_front = (uint16_t*)malloc(_camWidth*_camHeight*sizeof(uint16_t));

	thresh_mid = (uint8_t*)malloc(_camWidth*_camHeight);
	thresh_front = (uint8_t*)malloc(_camWidth*_camHeight);

	mmtt_values["showrawdepth"] = &val_showrawdepth;
	mmtt_values["showfps"] = &val_showfps;
	mmtt_values["flipx"] = &val_flipx;
	mmtt_values["flipy"] = &val_flipy;
	mmtt_values["showregionhits"] = &val_showregionhits;
	mmtt_values["showmask"] = &val_showmask;
	mmtt_values["showregionmap"] = &val_showregionmap;
	mmtt_values["showregionrects"] = &val_showregionrects;
	mmtt_values["showgreyscaledepth"] = &val_showgreyscaledepth;
	mmtt_values["usemask"] = &val_usemask;
	mmtt_values["tilt"] = &val_tilt;
	mmtt_values["edge_left"] = &val_edge_left;
	mmtt_values["edge_right"] = &val_edge_right;
	mmtt_values["edge_top"] = &val_edge_top;
	mmtt_values["edge_bottom"] = &val_edge_bottom;
	mmtt_values["depth_front"] = &val_depth_front;
	mmtt_values["depth_detect_top"] = &val_depth_detect_top;
	mmtt_values["depth_detect_bottom"] = &val_depth_detect_bottom;
	mmtt_values["depth_align"] = &val_depth_align;
	mmtt_values["blob_filter"] = &val_blob_filter;
	mmtt_values["blob_param1"] = &val_blob_param1;
	mmtt_values["blob_maxsize"] = &val_blob_maxsize;
	mmtt_values["blob_minsize"] = &val_blob_minsize;
	mmtt_values["confidence"] = &val_confidence;
	mmtt_values["depthfactor"] = &val_depthfactor;
	mmtt_values["expandfactor"] = &val_expandfactor;
	mmtt_values["expand_xmin"] = &val_expand_xmin;
	mmtt_values["expand_xmax"] = &val_expand_xmax;
	mmtt_values["expand_ymin"] = &val_expand_ymin;
	mmtt_values["expand_ymax"] = &val_expand_ymax;
	mmtt_values["autowindow"] = &val_auto_window;

	_camSize = cvSize(_camWidth,_camHeight);
	_camSizeInBytes = _camWidth * _camHeight * _camBytesPerPixel;
	_tmpGray = cvCreateImage( _camSize, 8, 1 ); // allocate a 1 channel byte image
	_maskImage = cvCreateImage( _camSize, IPL_DEPTH_8U, 1 ); // allocate a 1 channel byte image

	clearImage(_maskImage);

	_regionsImage = cvCreateImage( _camSize, IPL_DEPTH_8U, 1 ); // allocate a 1 channel byte image
	_tmpRegionsColor = cvCreateImage( _camSize, IPL_DEPTH_8U, 3 );

	_tmpThresh = cvCreateImageHeader( _camSize, IPL_DEPTH_8U, 1 );

	_ffImage = cvCreateImageHeader( _camSize, IPL_DEPTH_8U, 3 );

	_htmldir = MmttPath("html");
	_httpserver = NULL;

	SetOscClientList(_Tuio1_25_OscClientList,_Tuio1_25_Clients);
	SetOscClientList(_Tuio1_2_OscClientList,_Tuio1_2_Clients);
	SetOscClientList(_Tuio2_OscClientList,_Tuio2_Clients);

	std::string ss;

	_ffpixels = NULL;
	_ffpixelsz = 0;

	if ( _patchFile != "" ) {
		std::string err = LoadPatch(_patchFile);
		if ( err != "" ) {
			DEBUGPRINT(("LoadPatch of %s failed!?  err=%s",_patchFile.c_str(),err.c_str()));
			std::string msg = NosuchSnprintf("*** Error ***\n\nLoadPatch of %s failed\n%s",_patchFile.c_str(),err.c_str());
			ErrorPopup(msg.c_str());
			exit(1);
		}
	}

	if ( _do_sharedmem ) {
		_sharedmem_image = setup_shmem_for_image();
		_sharedmem_outlines = setup_shmem_for_outlines();
	} else {
		_sharedmem_image = NULL;
		_sharedmem_outlines = NULL;
	}

	if ( _do_initialalign ) {
		startAlign(true);
	}
	_status = "";
}

void
MmttServer::StartStuff()
{
	_httpserver = new NosuchDaemon(-1,"",NULL,
		_jsonport,_htmldir,this);
}

void
MmttServer::init_regular_values() {

	// pre-compute cvScalar values, optimization
	for ( int i=0; i<=MAX_REGION_ID; i++ ) {
		RGBcolor color = region2color[i];
		// NOTE: swapping the R and B values, since OpenCV defaults to BGR
		region2cvscalar[i] = CV_RGB(color.b,color.g,color.r);
	}

	// These should NOT be persistent in a saved patch
	val_showrawdepth = MmttValue(0,0,1,false);
	val_showregionrects = MmttValue(1,0,1,false);
	val_showfps = MmttValue(0,0,1,false);
	val_showregionhits = MmttValue(1,0,1,false);
	val_showregionmap = MmttValue(0,0,1,false);
	val_showgreyscaledepth = MmttValue(0,0,1,false);
	val_showmask = MmttValue(0,0,1,false);
	val_usemask = MmttValue(1,0,1,false);

	// These should be persistent in a saved patch
	val_tilt = MmttValue(0,-12.9,30.0);
	val_edge_left = MmttValue(0,0,639);
	val_edge_right = MmttValue(639,0,639);
	val_edge_top = MmttValue(0,0,479);
	val_edge_bottom = MmttValue(479,0,479);
	val_depth_front = MmttValue(0,0,3000);         // mm
	val_auto_window = MmttValue(80,0,400); // mm
	val_blob_filter = MmttValue(1,0,1);
	val_blob_param1 = MmttValue(100,0,250.0);
	val_blob_maxsize = MmttValue(10000.0,0,15000.0);
	val_blob_minsize = MmttValue(/* 65.0 */ 150,0,5000.0);
	val_confidence = MmttValue(200,0,4000.0);
	val_depthfactor = MmttValue(1.0,0.01,5.0);
	val_expandfactor = MmttValue(1.0,0.01,2.0);
	val_expand_xmin = MmttValue(0.0,0.00,1.0);
	val_expand_xmax = MmttValue(1.0,0.00,1.0);
	val_expand_ymin = MmttValue(0.0,0.00,1.0);
	val_expand_ymax = MmttValue(1.0,0.00,1.0);
	val_flipx = MmttValue(0,0,1);
	val_flipy = MmttValue(0,0,1);

	DEBUGPRINT1(("TEMPORARY blob_minsize HACK - used to be 65.0"));
}

void
MmttServer::init_camera_values() {

	val_depth_detect_top = MmttValue(camera->default_depth_detect_top(),0,3000);    // mm
	val_depth_detect_bottom = MmttValue(camera->default_depth_detect_top(),0,3000); // mm
	val_depth_align = MmttValue(camera->default_depth_detect_top(),0,3000); // mm
}

void MmttServer::InitOscClientLists() {
	if ( first_client_option ) {
		// If you explicitly set any clients, remove the default
		_Tuio1_2_OscClientList = "";
		_Tuio1_25_OscClientList = "";
		_Tuio2_OscClientList = "";
		first_client_option = FALSE;
	}
}


MmttServer::~MmttServer() {

	shutting_down = TRUE;

	DEBUGPRINT1(("MmttServer destructor!\n"));
	delete _httpserver;

	camera->Shutdown();
}

void MmttServer::ErrorPopup(wchar_t const* msg) {
	MessageBoxW(NULL,msg,L"MultiMultiTouchTouch",MB_OK);
}

void MmttServer::ErrorPopup(const char* msg) {
	MessageBoxA(NULL,msg,"MultiMultiTouchTouch",MB_OK);
}

static vector<string>
tokenize(const string& str,const string& delimiters) {

	vector<string> tokens;
    	
	// skip delimiters at beginning.
	string::size_type lastPos = str.find_first_not_of(delimiters, 0);
    	
	// find first "non-delimiter".
	string::size_type pos = str.find_first_of(delimiters, lastPos);

	while (string::npos != pos || string::npos != lastPos) {
    	// found a token, add it to the vector.
    	tokens.push_back(str.substr(lastPos, pos - lastPos));
		
    	// skip delimiters.  Note the "not_of"
    	lastPos = str.find_first_not_of(delimiters, pos);
		
    	// find next "non-delimiter"
    	pos = str.find_first_of(delimiters, lastPos);
	}

	return tokens;
}

void MmttServer::SetOscClientList(std::string& clientlist,std::vector<OscSender*>& clientvector)
{
	vector<string> tokens = tokenize(clientlist,";:");
	for ( vector<string>::iterator it=tokens.begin(); it != tokens.end(); ) {
		string ahost = *it++;
		if ( it == tokens.end() )
			break;
		string aport = *it++;

		OscSender *o = new OscSender();
		o->setup(ahost,atoi(aport.c_str()));
		clientvector.push_back(o);
	}

}

UT_SharedMem*
MmttServer::setup_shmem_for_outlines() {

	// Create Shared memory
	unsigned int headersize = sizeof(MMTT_SharedMemHeader);
	unsigned int outlinesize = NUM_BUFFS * MMTT_OUTLINES_MAX * sizeof(OutlineMem);
	unsigned int pointsize =   NUM_BUFFS * MMTT_POINTS_MAX * sizeof(PointMem);
	unsigned int totalsize = headersize + outlinesize + pointsize;

	UT_SharedMem* mem = new UT_SharedMem(_sharedmemname.c_str(), totalsize); // figure out the size based on the OP

	DEBUGPRINT(("Shared Memory created, name=%s size=%d",_sharedmemname.c_str(),totalsize));

	UT_SharedMemError shmerr = mem->getErrorState();
	if (shmerr != UT_SHM_ERR_NONE) { 
		DEBUGPRINT(("Error when creating SharedMem?? err=%d",shmerr));
		return NULL;
	}

	mem->lock();
	void *data = mem->getMemory();
	if ( !data ) {
		DEBUGPRINT(("NULL returned from getMemory of Shared Memory!?  Disabling shmem"));
		return NULL;
	}
	MMTT_SharedMemHeader* h = (MMTT_SharedMemHeader*)data;
	h->xinit();

	mem->unlock();
	return mem;
}

UT_SharedMem*
MmttServer::setup_shmem_for_image() {

	unsigned int shmemWidth = _camWidth;
	unsigned int shmemHeight = _camHeight;
	// Create Shared memory
	unsigned int headersize = sizeof(TOP_SharedMemHeader);
	unsigned int bytes_per_channel = 1;  // GL_UNSIGNED_BYTE
	unsigned int nchannels = 3;  // GL_RGB
	unsigned int imagesize = shmemWidth * shmemHeight * nchannels * bytes_per_channel;
	DEBUGPRINT1(("headersize=%d imagesize=%d",headersize,imagesize));
	unsigned int totalsize = headersize + imagesize;

	UT_SharedMem* mem = new UT_SharedMem("mmtt", totalsize); // figure out the size based on the OP
	UT_SharedMemError shmerr = mem->getErrorState();
	if (shmerr != UT_SHM_ERR_NONE) { 
		DEBUGPRINT(("Error when creating SharedMem for image?? err=%d",shmerr));
		return NULL;
	}

	mem->lock();
	void *data = mem->getMemory();
	if ( !data ) {
		DEBUGPRINT(("NULL returned from getMemory of Shared Memory!?  Disabling shmem"));
		return NULL;
	}
	TOP_SharedMemHeader* h = (TOP_SharedMemHeader*)data;

    // Magic number to make sure we are looking at the correct memory
    // must be set to TOP_SHM_MAGIC_NUMBER (0xe95df673)
    h->magicNumber = TOP_SHM_MAGIC_NUMBER; 
    // header, must be set to TOP_SHM_VERSION_NUMBER
    h->version = TOP_SHM_VERSION_NUMBER;
    h->width = shmemWidth;
    h->height = shmemHeight;
    // X aspect of the image
    h->aspectx = 1.0;
    // Y aspect of the image
    h->aspecty = 1.0;
    // Format of the image data in memory (RGB, RGBA, BGR, BGRA etc.)
    h->dataFormat = GL_RGB;
    h->dataType = GL_UNSIGNED_BYTE; 
    // The desired pixel format of the texture to be created in Touch (RGBA8, RGBA16, RGBA32 etc.)
    // h->pixelFormat = GL_RGBA8;
    h->pixelFormat = GL_RGB8;
	h->dataOffset = h->calcDataOffset();

	// Just a dummy image/gradient so it'll show something
	unsigned char* img = (unsigned char*) h->getImage();
	for ( int y=0; y<h->height; y++ ) {
		unsigned char *p = img + (y*h->width*nchannels*bytes_per_channel);
		for ( int x=0; x<h->width; x++ ) {
			// BGRA
			*p++ = y % 255;
			*p++ = x % 255;
			*p++ = (x*y)%255;
			*p++ = 255;
		}
	}

	mem->unlock();
	return mem;
}

void
MmttServer::shmem_lock_and_update_outlines(int nactive, int numblobs, std::vector<int> &blob_sid, std::vector<MmttRegion*> &blob_region, std::vector<CvPoint> &blob_center)
{
	if ( _sharedmem_outlines == NULL )
		return;

	bool doupdate = true;

	_sharedmem_outlines->lock();

	void *data = _sharedmem_outlines->getMemory();
	if ( !data ) {
		DEBUGPRINT(("NULL returned from getMemory of Shared Memory!?  Disabling sharedmem_outlines"));
		_sharedmem_outlines->unlock();
		_sharedmem_outlines = NULL;
		return;
	}
	MMTT_SharedMemHeader* h = (MMTT_SharedMemHeader*) data;

	if ( h->buff_being_constructed != BUFF_UNSET ) {
		if ( h->buff_to_display_next != BUFF_UNSET ) {
			// Both buffers are full.  buff_being_constructed is the newer one,
			// and buff_to_display_next is the older one.
			// Swap them, so the newer one is going to be displayed next, and
			// we're going to build a new one in the older buffer.
			buff_index t = h->buff_to_display_next;
			h->buff_to_display_next = h->buff_being_constructed;
			h->buff_being_constructed = t;
			// Both buffers are still in use.
		} else {
			// The to_display_next one was used and is now UNSET,
			// so shift the being_constructed into it so it'll be displayed next,
			// and find an empty buffer for buff_being_constructed. 
			h->buff_to_display_next = h->buff_being_constructed;
			h->buff_being_constructed = h->grab_unused_buffer();
		}
	} else {
			h->buff_being_constructed = h->grab_unused_buffer();
	}

	if ( h->buff_being_constructed == BUFF_UNSET ) {
		DEBUGPRINT(("Hey! No buffer available in shmem_update_outlines!"));
		doupdate = false;
	}

	// XXX - Do we need to have the memory locked when
	// updating the outlines/etc?  Not sure.  Might only be needed when changing the
	// buff_* pointers?

	shmem_update_outlines(h, nactive, numblobs, blob_sid, blob_region, blob_center);

	_sharedmem_outlines->unlock();

	if ( ! doupdate ) {
		DEBUGPRINT(("NOT UPDATING in shmem_update_outlines because !doupdate"));
		return;
	}

	// TODO - Here we should update all the curors/blobs/points
	// in the buffer h->buff_being_constructed

	_sharedmem_outlines->lock();

	if ( h->buff_to_display_next == BUFF_UNSET ) {
		h->buff_to_display_next = h->buff_being_constructed;
		h->buff_being_constructed = BUFF_UNSET;
	} else {
	}

	// The h->buff_inuse value for the buffer remains set to true.
	if ( h->buff_inuse[h->buff_to_display_next] != true ) {
		DEBUGPRINT(("UNEXPECTED in shmem_update_outlines!! h->buff_inuse isn't true?"));
	}

	_sharedmem_outlines->unlock();
}

int
tuioSessionId(MmttRegion* r, int session_id)
{
	// internal Region ids start at 2.
	return r->_first_sid + session_id;
}

// This normalization results in the range (0,0)->(1,1), relative to the region space
void
normalize_region_xy(float& x, float& y, CvRect& rect, float expandfactor = 1.0f, float e_xmin=0.0f, float e_xmax=1.0f, float e_ymin=0.0f, float e_ymax=1.0f)
{
	x = x - rect.x;
	y = y - rect.y;
	x = x / rect.width;
	y = 1.0f - (y / rect.height);

#ifdef ORIGINAL_EXPANDFACTOR
	if ( expandfactor != 1.0 ) {
		// A small palette makes it difficult to access
		// all the way to 0.0 or 1.0 in the regions, so expandfactor lets you adjust for that
		x = ((x - 0.5f) * expandfactor) + 0.5f;
		y = ((y - 0.5f) * expandfactor) + 0.5f;
	}
#else
	x = ((x - e_xmin) * (1.0f / (e_xmax-e_xmin)));
	y = ((y - e_ymin) * (1.0f / (e_ymax-e_ymin)));
#endif

	if ( x > 1.0 ) {
		x = 1.0;
	} else if ( x < 0.0 ) {
		x = 0.0;
	}

	if ( y > 1.0 ) {
		y = 1.0;
	} else if ( y < 0.0 ) {
		y = 0.0;
	}

}

// This normalization results in the range (-1,-1)->(1,1), relative to the outline center
void
normalize_outline_xy(float& x, float& y, float& blobcenterx, float& blobcentery, CvRect& rect)
{
	normalize_region_xy(x,y,rect);
	x -= blobcenterx;
	y -= blobcentery;
	// At this point we're centered on the blobcenter, with range (-1,-1) to (1,1)
	x = (x * 2.0f);
	y = (y * 2.0f);
}

void
MmttServer::shmem_update_outlines(MMTT_SharedMemHeader* h,
	int nactive, int numblobs, std::vector<int> &blob_sid,
	std::vector<MmttRegion*> &blob_region, std::vector<CvPoint> &blob_center)
{
	buff_index buff = h->buff_being_constructed;
	h->clear_lists(buff);

	// print_buff_info("shmem_update_outlines start",h);

	for ( int i=0; i<numblobs; i++ ) {

		MmttRegion* r = blob_region[i];
		int sid = blob_sid[i];
		if ( r == NULL || sid < 0 ) {
			continue;
		}
		CBlob *blob = _newblobresult->GetBlob(i);

		int tuio_sid = tuioSessionId(r,sid);
		MmttSession* sess = r->_sessions[sid];

		CvRect blobrect = blob->GetBoundingBox();
		CvRect regionrect = r->_rect;
		CvPoint blobcenter = blob_center[i];

		float blobcenterx = (float)blobcenter.x;
		float blobcentery = (float)blobcenter.y;
		normalize_region_xy(blobcenterx,blobcentery,regionrect);

		float depth = (float)sess->_depth_normalized;

		float blobarea = float(blobrect.width * blobrect.height) / (regionrect.width*regionrect.height);

		CBlobContour* contour = blob->GetExternalContour();
		if ( ! contour ) {
			DEBUGPRINT(("HEY!  contour==NULL?  in shmem_update_outlines"));
			continue;
		}
		t_chainCodeList contourpoints = contour->GetContourPoints();
		if ( ! contourpoints ) {
			// happens when there are 0 contour points
			continue;
		}
		int npoints = contourpoints->total;

		h->addOutline(buff,r->id,tuio_sid,blobcenterx,blobcentery,depth,blobarea,npoints);

		if ( npoints > 0 ) {
			for(int i = 0; i < npoints; i++) {
				CvPoint pt0 = *CV_GET_SEQ_ELEM( CvPoint, contourpoints, i );
				float x = (float) pt0.x;
				float y = (float) pt0.y;
				normalize_outline_xy(x,y,blobcenterx,blobcentery,regionrect);
				h->addPoint(buff,x,y,depth);
			}
		}
	}

	// if ( h->numoutlines[buff] > 0 ) {
	// 	DEBUGPRINT(("MMTT_Kinect - h=%lx buff=%d number of outlines = %d",(long)h,buff,h->numoutlines[buff]));
	// }

	// print_buff_info("shmem_update_outlines end",h);

	h->seqnum = _tuio_fseq++;
	h->lastUpdateTime = timeGetTime();
	h->active = 1;
}

void
MmttServer::shmem_lock_and_update_image(unsigned char* psource) {
	NosuchAssert(_sharedmem_image != NULL);
	_sharedmem_image->lock();
	void *data = _sharedmem_image->getMemory();
	if ( !data ) {
		DEBUGPRINT(("NULL returned from getMemory of Shared Memory!?  Disabling shmem"));
		_sharedmem_image = NULL;
		return;
	}
	TOP_SharedMemHeader* h = (TOP_SharedMemHeader*)data;

	shmem_update_image(h,psource);

	_sharedmem_image->unlock();
}

void
MmttServer::shmem_update_image(TOP_SharedMemHeader* h, unsigned char* psource) {

	unsigned char* pdest = (unsigned char*) h->getImage();
	for ( int y=0; y<h->height; y++ ) {
		unsigned char *ps = psource + (y*h->width*3);
		unsigned char *pd = pdest + (y*h->width*3);
		for ( int x=0; x<h->width; x++ ) {
			*pd++ = *ps++;
			*pd++ = *ps++;
			*pd++ = *ps++;
		}
	}
}

void
MmttServer::SendAllOscClients(OscBundle& bundle, std::vector<OscSender *> &oscClients)
{
	vector<OscSender*>::iterator it;
	for ( it=oscClients.begin(); it != oscClients.end(); it++ ) {
		OscSender *sender = *it;
		sender->sendBundle(bundle);
	}
}

void MmttServer::check_json_and_execute()
{
	pthread_mutex_lock(&json_mutex);
	if (json_pending) {
		// Execute json stuff and generate response
		json_result = processJson(json_method, json_params, json_id);
		DEBUGPRINT1(("AFTER processJson, result=%s\n",json_result.c_str()));
		json_pending = FALSE;
		pthread_cond_signal(&json_cond);
	}
	pthread_mutex_unlock(&json_mutex);
}

#define NO_FLIP 99

int MmttServer::get_flip_mode() {
	int flip_mode = NO_FLIP;
	if (val_flipx.value && val_flipy.value) {
		flip_mode = -1;
	} else if (val_flipx.value) {
		flip_mode = 1;
	} else if (val_flipx.value) {
		flip_mode = 0;
	}
	return flip_mode;
}

void MmttServer::flip_images() {
	int flip_mode = get_flip_mode();
	if (flip_mode != NO_FLIP) {
		// Flip the image in depth_mid
		IplImage* image_depth_mid = cvCreateImageHeader( _camSize, IPL_DEPTH_8U, _camBytesPerPixel );
		image_depth_mid->origin = 1;
		image_depth_mid->imageData = (char*)depth_mid;
		cvFlip(image_depth_mid,image_depth_mid,flip_mode);
		cvReleaseImageHeader(&image_depth_mid);

		// Flip the image in thresh_mid, which is a one-byte (black/white) image
		IplImage* image_thresh_mid = cvCreateImageHeader( _camSize, IPL_DEPTH_8U, 1 );
		image_thresh_mid->origin = 1;
		image_thresh_mid->imageData = (char*)thresh_mid;
		cvFlip(image_thresh_mid,image_thresh_mid,flip_mode);
		cvReleaseImageHeader(&image_thresh_mid);

		// Flip the image in depthmm_mid, which is UINT16 values
		IplImage* image_depthmm_mid = cvCreateImageHeader( _camSize, IPL_DEPTH_16U, 1 );
		image_depthmm_mid->origin = 1;
		image_depthmm_mid->imageData = (char*)depthmm_mid;
		cvFlip(image_depthmm_mid,image_depthmm_mid,flip_mode);
		cvReleaseImageHeader(&image_depthmm_mid);
	}
}

void MmttServer::analyze_depth_images()
{
	// pthread_mutex_lock(&gl_backbuf_mutex);

	uint8_t *tmp;
	uint16_t *tmp16;

	// This buffering was needed when using the freenect library, but
	// with the Microsoft SDK, I don't thing it's actually needed (as much).
	// However, it's not really expensive (just twiddling some pointers),
	// and I don't want to break anything, so I'm leaving it.

	tmp = depth_front;
	depth_front = depth_mid;
	depth_mid = tmp;

	tmp16 = depthmm_front;
	depthmm_front = depthmm_mid;
	depthmm_mid = tmp16;

	tmp = thresh_front;
	thresh_front = thresh_mid;
	thresh_mid = tmp;

	unsigned char *surfdata = NULL;

	if ( _ffpixels == NULL || _ffpixelsz < _camSizeInBytes ) {
		_ffpixels = (unsigned char *)malloc(_camSizeInBytes);
		_ffpixelsz = _camSizeInBytes;
	}
	if ( depth_front != NULL ) {
		surfdata = depth_front;
	}

	if ( _ffpixels != NULL && surfdata != NULL ) {

		_ffImage->origin = 1;
		_ffImage->imageData = (char *)_ffpixels;

		_tmpThresh->origin = 1;
		_tmpThresh->imageData = (char *)thresh_front;

		if ( surfdata ) {
			memcpy(_ffpixels,surfdata,_camSizeInBytes);
			if ( ! val_showrawdepth.value ) {
				analyzePixels();
			}
		} else {
#if 0
			memset(_ffpixels,0,_camSizeInBytes);
#endif
		}

		// Now put whatever we want to show into the _ffImage/_ffpixels image

		if ( surfdata && val_showrawdepth.value ) {
			// Doesn't seem to be needed...
			// memcpy(_ffpixels,surfdata,_camSizeInBytes);
		}

		if ( val_showregionrects.value ) {
			// When showing the region rectangles, nothing else is shown.
			showRegionRects();
		}

		if ( val_showregionmap.value ) {
			if ( ! _regionsDefinedByPatch ) {
				// When showing the colored regions, nothing else is shown.
				copyRegionsToColorImage(_regionsImage,_ffpixels,TRUE,FALSE,FALSE);
			}
		} else {
			if ( val_showregionhits.value ) {
				showRegionHits();
				showBlobSessions();
			}

			if ( val_showmask.value ) {
				showMask();
			} else if ( val_showregionmap.value ) {
				copyRegionsToColorImage(_regionsImage,_ffpixels,TRUE,FALSE,FALSE);
			}
		}
	}
}

void MmttServer::draw_depth_image(unsigned char *pix) {

// #define DRAW_BOX_TO_DEBUG_THINGS
#ifdef DRAW_BOX_TO_DEBUG_THINGS
	glColor4f(0.0,1.0,0.0,0.5);
	glLineWidth((GLfloat)10.0f);
	glBegin(GL_LINE_LOOP);
	glVertex3f(-0.8f, 0.8f, 0.0f);	// Top Left
	glVertex3f( 0.8f, 0.8f, 0.0f);	// Top Right
	glVertex3f( 0.8f,-0.8f, 0.0f);	// Bottom Right
	glVertex3f(-0.8f,-0.8f, 0.0f);	// Bottom Left
	glEnd();

	glColor4f(1.0,1.0,1.0,1.0);
#endif

	glPixelStorei (GL_UNPACK_ROW_LENGTH, _camWidth);
 
	static bool initialized = false;
	static GLuint depthTextureId;
	if ( ! initialized ) {
		initialized = true;
	    glGenTextures( 1, &depthTextureId );
	}

	glEnable( GL_TEXTURE_2D );

	glEnable(GL_BLEND); 

	// glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  // original

	// We want the black in the texture image
	// to NOT wipe out other things.
	glBlendFunc(GL_ONE, GL_ONE);

	if ( pix != NULL ) {

	    glBindTexture( GL_TEXTURE_2D, depthTextureId );

		// The image we sent to Spout is horizontally flipped.  This is an attempt to fix that
		// XXX - should be flipping pix
		IplImage* pixflipped = cvCreateImageHeader( _camSize, IPL_DEPTH_8U, 3 );
		pixflipped->origin = 1;
		pixflipped->imageData = (char*)pix;
		cvFlip(pixflipped,pixflipped,1);

		glTexImage2D(GL_TEXTURE_2D, 0,GL_RGB,
			_camWidth, _camHeight,
			0, GL_RGB, GL_UNSIGNED_BYTE, pix);

		if ( _do_sharedmem ) {
			shmem_lock_and_update_image(pix);
		}

		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);

		glPushMatrix();

		glColor4f(1.0,1.0,1.0,0.5);
		glBegin(GL_QUADS);
		glTexCoord2d(0.0,0.0); glVertex3f(-1.0f, 1.0f, 0.0f);	// Top Left
		glTexCoord2d(1.0,0.0); glVertex3f( 1.0f, 1.0f, 0.0f);	// Top Right
		glTexCoord2d(1.0,1.0); glVertex3f( 1.0f,-1.0f, 0.0f);	// Bottom Right
		glTexCoord2d(0.0,1.0); glVertex3f(-1.0f,-1.0f, 0.0f);	// Bottom Left
		glEnd();			

		glPopMatrix();

		glBindTexture(GL_TEXTURE_2D, 0);

		_spoutDepthSender.SendTexture(depthTextureId, GL_TEXTURE_2D, _camWidth, _camHeight, false, 0);

		cvReleaseImageHeader(&pixflipped);
	}

	glDisable( GL_TEXTURE_2D );

	// Should I disable GL_BLEND here?

#ifdef DRAW_BOX_TO_DEBUG_THINGS
	glColor4f(1.0,0.0,0.0,0.5);
	glLineWidth((GLfloat)10.0f);
	glBegin(GL_LINE_LOOP);
	glVertex3f(-0.5f, 0.5f, 0.0f);	// Top Left
	glVertex3f( 0.5f, 0.5f, 0.0f);	// Top Right
	glVertex3f( 0.5f,-0.5f, 0.0f);	// Bottom Right
	glVertex3f(-0.5f,-0.5f, 0.0f);	// Bottom Left
	glEnd();
#endif

}

void MmttServer::swap_buffers() {

	if ( continuousAlign == true && (
		registrationState == 300
		|| registrationState == 310
		|| registrationState == 311
		|| registrationState == 320
		|| registrationState == 330
		) ) {
		DEBUGPRINT1(("registrationState = %d",registrationState));
			// do nothing - this avoids screen blinking/etc with in continuousAlign registration
	} else {
		SwapBuffers(g.hdc);
	}
}

void MmttServer::draw_begin() {
	glMatrixMode(GL_MODELVIEW);	// Select The Modelview Matrix
	glLoadIdentity();			// Reset The Modelview Matrix
	glPushMatrix();

	// this is tweaked so image fills the window.
	gluLookAt(  0, 0, 2.43,
		0, 0, 0,
		0, 1, 0); 

	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
}

void MmttServer::draw_end() {
	glPopMatrix();
	glColor4f(1.0,1.0,1.0,1.0);
	swap_buffers();
}

void MmttServer::draw_color_image() {

	IplImage* cimage = camera->colorimage();
	if ( cimage == NULL ) {
		return;
	}

	int flip_mode = get_flip_mode();
	if (flip_mode != NO_FLIP) {
		cvFlip(cimage,cimage,flip_mode);
	}

#ifdef DEBUG_COLOR
	glColor4f(1.0,0.0,0.0,0.5);
	glLineWidth((GLfloat)10.0f);
	glBegin(GL_LINE_LOOP);
	glVertex3f(-0.8f, 0.8f, 0.0f);	// Top Left
	glVertex3f( 0.8f, 0.8f, 0.0f);	// Top Right
	glVertex3f( 0.8f,-0.8f, 0.0f);	// Bottom Right
	glVertex3f(-0.8f,-0.8f, 0.0f);	// Bottom Left
	glEnd();

	glColor4f(1.0,1.0,1.0,1.0);
#endif

	glPixelStorei (GL_UNPACK_ROW_LENGTH, _camWidth);
 
	unsigned char *pix = (unsigned char *)(cimage->imageData);

	static bool initialized = false;
	static GLuint colorTextureId;
	if ( ! initialized ) {
		initialized = true;
	    glGenTextures( 1, &colorTextureId );
	}

	glEnable( GL_TEXTURE_2D );

	glEnable(GL_BLEND); 

	// glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  // original

	// We want the black in the texture image
	// to NOT wipe out other things.
	glBlendFunc(GL_ONE, GL_ONE);

	if ( pix != NULL ) {

	    glBindTexture( GL_TEXTURE_2D, colorTextureId );

		// The use of GL_BGR_EXT here is suspect - the Realsense camera SDK seems to put out BGR format.
		// See the comment in pxcimage.h about PIXEL_FORMAT_RGB24 (it says it uses BGR on little-endian machines)
		glTexImage2D(GL_TEXTURE_2D, 0,GL_RGB,
			_camWidth, _camHeight,
			0, GL_BGR_EXT, GL_UNSIGNED_BYTE, pix);

		if ( _do_sharedmem ) {
			shmem_lock_and_update_image(pix);
		}

		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);

		glPushMatrix();

		glColor4f(1.0,1.0,1.0,0.5);
		glBegin(GL_QUADS);
		GLfloat x0 = -1.0f;
		GLfloat y0 = 1.0f;
		GLfloat x1 = 1.0f;
		GLfloat y1 = -1.0f;
		glTexCoord2d(0.0,0.0); glVertex3f( x0, y0, 0.0f);	// Top Left
		glTexCoord2d(1.0,0.0); glVertex3f( x1, y0, 0.0f);	// Top Right
		glTexCoord2d(1.0,1.0); glVertex3f( x1, y1, 0.0f);	// Bottom Right
		glTexCoord2d(0.0,1.0); glVertex3f( x0, y1, 0.0f);	// Bottom Left
		glEnd();			

		glPopMatrix();

		glBindTexture(GL_TEXTURE_2D, 0);

		_spoutColorSender.SendTexture(colorTextureId, GL_TEXTURE_2D, _camWidth, _camHeight, false, 0);
	}

	glDisable( GL_TEXTURE_2D );

}

static std::string
OscMessageToJson(OscMessage& msg) {
	std::string s = NosuchSnprintf("{ \"address\" : \"%s\", \"args\" : [ ", msg.getAddress().c_str());
	std::string sep = "";
	int nargs = msg.getNumArgs();
	for ( int n=0; n<nargs; n++ ) {
		s += sep;
		ArgType t = msg.getArgType(n);
		switch (t) {
		case TYPE_INT32:
			s += NosuchSnprintf("%d",msg.getArgAsInt32(n));
			break;
		case TYPE_FLOAT:
			s += NosuchSnprintf("%f",msg.getArgAsFloat(n));
			break;
		case TYPE_STRING:
			s += NosuchSnprintf("\"%s\"",msg.getArgAsString(n).c_str());
			break;
		default:
			DEBUGPRINT(("Unable to handle type=%d in OscMessageToJson!",t));
			break;
		}
		sep = ",";
	}
	s += " ] }";
	return s;
}

static std::string
OscBundleToJson(OscBundle& bundle) {
	std::string s = "{ \"messages\" : [";
	std::string sep = "";
	int nmessages = bundle.getMessageCount();
	for ( int n=0; n<nmessages; n++ ) {
		s += sep;
		s += OscMessageToJson(bundle.getMessageAt(n));
		sep = ", ";
	}
	s += " ] }";
	return s;
}

void
MmttServer::SendOscToAllWebSocketClients(OscBundle& bundle)
{
	std::string msg = OscBundleToJson(bundle);
	if ( _httpserver ) {
		_httpserver->SendAllWebSocketClients(msg);
	}
}


std::string
MmttServer::submitJson(std::string method, cJSON *params, const char *id) {

	pthread_mutex_lock(&json_mutex);

	std::string result;

	json_pending = TRUE;
	json_method = method;
	json_params = params;
	json_id = id;
	while ( json_pending ) {
		pthread_cond_wait(&json_cond, &json_mutex);
	}
	result = json_result;

	pthread_mutex_unlock(&json_mutex);

	return result;
}

static u_long LookupAddress(const char* pcHost)
{
    u_long nRemoteAddr = inet_addr(pcHost);
    if (nRemoteAddr == INADDR_NONE) {
        // pcHost isn't a dotted IP, so resolve it through DNS
        hostent* pHE = gethostbyname(pcHost);
        if (pHE == 0) {
            return INADDR_NONE;
        }
        nRemoteAddr = *((u_long*)pHE->h_addr_list[0]);
    }

    return nRemoteAddr;
}

#if !defined(_WINSOCK2API_) 
// Winsock 2 header defines this, but Winsock 1.1 header doesn't.  In
// the interest of not requiring the Winsock 2 SDK which we don't really
// need, we'll just define this one constant ourselves.
#define SD_SEND 1
#endif

// direction = -1  (decrease)
// direction =  0  (toggle)
// direction =  1  (increase)
std::string
MmttServer::AdjustValue(cJSON *params, const char *id, int direction) {

	static std::string errstr;  // So errstr.c_str() stays around

	cJSON *c_name = cJSON_GetObjectItem(params,"name");
	if ( ! c_name ) {
		return error_json(-32000,"Missing name argument",id);
	}
	if ( c_name->type != cJSON_String ) {
		return error_json(-32000,"Expecting string type in name argument to mmtt_set",id);
	}
	std::string nm = std::string(c_name->valuestring);

	map<std::string, MmttValue*>::iterator it = mmtt_values.find(nm);
	if ( it == mmtt_values.end() ) {
		errstr = NosuchSnprintf("No kinect parameter with that name - %s",nm.c_str());
		return error_json(-32000,errstr.c_str(),id);
	}
	MmttValue* kv = it->second;

	if ( direction != 0 ) {
		cJSON *c_amount = cJSON_GetObjectItem(params,"amount");
		if ( ! c_amount ) {
			return error_json(-32000,"Missing amount argument",id);
		}
		if ( c_amount->type != cJSON_Number ) {
			return error_json(-32000,"Expecting number type in amount argument to mmtt_set",id);
		}
		kv->set_value(kv->value + direction * c_amount->valuedouble * ( kv->maxvalue - kv->minvalue) );
	} else {
		kv->set_value(! kv->value);
	}

	_updateValue(nm,kv);
	DEBUGPRINT1(("mmtt_SET name=%s value=%lf\n",nm.c_str(),kv->value));
	return NosuchSnprintf(
			"{\"jsonrpc\": \"2.0\", \"result\": %lf, \"id\": \"%s\"}", kv->value, id);
}

void
MmttServer::_updateValue(std::string nm, MmttValue* v) {
	if ( nm == "debug" ) {
		if ( v->value )
			NosuchDebugLevel = 1;
		else
			NosuchDebugLevel = 0;
	} else if ( nm == "tilt" ) {
		DEBUGPRINT(("Tilt value = %f",v->value));
		if ( ! camera->Tilt((int)v->value) ) {
			DEBUGPRINT(("This camera type doesn't have the ability to Tilt!"));
		}
	}
}

void
MmttServer::_toggleValue(MmttValue* v) {
		v->value = !v->value;
}

void
MmttServer::_stop_registration() {
	registrationState = 0;
	finishNewRegions();
	val_showregionmap.set_value(0.0);
}

static bool
has_invalid_char(const char *nm)
{
	for ( const char *p=nm; *p!='\0'; p++ ) {
		if ( ! isalnum(*p) )
			return TRUE;
	}
	return FALSE;
}

std::string
MmttServer::startAlign(bool reload) {
	if ( reload ) {
		std::string err = LoadPatch(_patchFile);
		if ( err != "" ) {
			return NosuchSnprintf("LoadPatch failed!?  err=%s",err.c_str());
		}
	}
	if ( _curr_regions.size() == 0 ) {
		return "No regions yet - do you need to load a patch?";
	}
	if ( val_usemask.value == 0 ) {
		return "Value of usemask is 0, no alignment can/should be done.";
	}
	registrationState = 300;
	continuousAlign = true;
	continuousAlignOkayCount = 0;
	continuousAlignOkayEnough = 10;
	return "";
}

std::string
MmttServer::processJson(std::string meth, cJSON *params, const char *id) {

	static std::string errstr;  // So errstr.c_str() stays around

	const char *method = meth.c_str();
	if ( strncmp(method,"mmtt_get",8) != 0 ) {
		DEBUGPRINT(("executeJson, method=%s\n",method));
	}
	// std::string m = meth;

	// NosuchPrintTime(NosuchSnprintf("Begin method=%s id=%s",method,id).c_str());
	if ( strcmp(method,"show") == 0 ) {
		// For some reason, if a window overlaps an
		// already-shown window, the BringWindowToTop
		// doesn't seem to do anything, so we
		// minimize it first to make sure it's on top.
		ShowWindow(g.hwnd, SW_SHOWMINIMIZED);
		ShowWindow(g.hwnd, SW_SHOWNORMAL);
		BringWindowToTop(g.hwnd);
		return ok_json(id);
	}
	if ( strcmp(method,"max") == 0 ) {
		ShowWindow(g.hwnd, SW_MAXIMIZE);
		BringWindowToTop(g.hwnd);
		return ok_json(id);
	}
	if ( strcmp(method,"hide") == 0 ) {
		ShowWindow(g.hwnd, SW_SHOWMINIMIZED);
		return ok_json(id);
	}
	if ( strcmp(method,"echo") == 0 ) {
		cJSON *c_nm = cJSON_GetObjectItem(params,"value");
		if ( ! c_nm ) {
			return error_json(-32000,"Missing value argument",id);
		}
		char *nm = c_nm->valuestring;
		return NosuchSnprintf(
			"{\"jsonrpc\": \"2.0\", \"result\": { \"value\": \"%s\" }, \"id\": \"%s\"}",nm,id);
	}
	if ( strcmp(method,"do_reset") == 0 ) {
		// Obsolete method.
		return ok_json(id);
	}
	if ( strcmp(method,"toggle_showmask") == 0 ) {
		_toggleValue(&val_showmask);
		return ok_json(id);
	}
	if ( strcmp(method,"toggle_autodepth") == 0 ) {
		autoDepth = !autoDepth;
		return ok_json(id);
	}
	if ( strcmp(method,"toggle_showregionmap") == 0 ) {
		_toggleValue(&val_showregionmap);
		return ok_json(id);
	}
	if ( strcmp(method,"do_setnextregion") == 0 ) {
		do_setnextregion = TRUE;
		return ok_json(id);
	}
	if ( strcmp(method,"toggle_bw") == 0 ) {
		doBW = !doBW;
		return ok_json(id);
	}
	if ( strcmp(method,"toggle_smooth") == 0 ) {
		doSmooth = !doSmooth;
		return ok_json(id);
	}
	if ( strcmp(method,"start") == 0 ) {
		val_showrawdepth.set_value(false);
		val_showregionrects.set_value(false);
		return ok_json(id);
	}
	if ( strcmp(method,"stop") == 0 ) {
		val_showrawdepth.set_value(true);
		val_showregionrects.set_value(true);
		return ok_json(id);
	}
	if ( strcmp(method,"start_registration") == 0 ) {
		registrationState = 100;
		return ok_json(id);
	}
	if ( strcmp(method,"autopoke") == 0 ) {
		std::string err = LoadPatch(_patchFile);
		if ( err != "" ) {
			std::string msg = NosuchSnprintf("LoadPatch failed!?  err=%s",err.c_str());
			return error_json(-32000,msg.c_str(),id);
		}
		if ( _curr_regions.size() == 0 ) {
			return error_json(-32000,"No regions yet - do you need to load a patch?",id);
		}
		if ( val_usemask.value == 0 ) {
			return error_json(-32000,"Usemask value is 0 - autopoke can't be done.",id);
		}
		registrationState = 300;
		return ok_json(id);
	}
	if ( strcmp(method,"align_start") == 0 ) {
		std::string err = startAlign(false);
		if ( err != "" ) {
			std::string msg = NosuchSnprintf("startAlign failed!?  err=%s",err.c_str());
			return error_json(-32000,msg.c_str(),id);
		}
		return ok_json(id);
	}
	if ( strcmp(method,"reload_and_align_start") == 0 ) {
		std::string err = startAlign(true);
		if ( err != "" ) {
			std::string msg = NosuchSnprintf("startAlign failed!?  err=%s",err.c_str());
			return error_json(-32000,msg.c_str(),id);
		}
		return ok_json(id);
	}
	if ( strcmp(method,"align_stop") == 0 ) {
		continuousAlign = false;
		return ok_json(id);
	}
	if ( strcmp(method,"align_isdone") == 0 ) {
		int r = continuousAlign ? 0 : 1;
		return NosuchSnprintf(
			"{\"jsonrpc\": \"2.0\", \"result\": %d, \"id\": \"%s\"}", r, id);
	}
	if ( strcmp(method,"autotiltpoke") == 0 ) {
		std::string err = LoadPatch(_patchFile);
		if ( err != "" ) {
			std::string msg = NosuchSnprintf("LoadPatch failed!?  err=%s",err.c_str());
			return error_json(-32000,msg.c_str(),id);
		}
		// if ( _curr_regions.size() == 0 ) {
		// 	return error_json(-32000,"No regions yet - do you need to load a patch?",id);
		// }
		registrationState = 1300;
		return ok_json(id);
	}
	if ( strcmp(method,"stop_registration") == 0 ) {
		_stop_registration();
		return ok_json(id);
	}
	if ( strcmp(method,"patch_save")==0 ) {
		cJSON *c_nm = cJSON_GetObjectItem(params,"name");
		if ( ! c_nm ) {
			return error_json(-32000,"Missing name argument",id);
		}
		char *nm = c_nm->valuestring;
		if ( has_invalid_char(nm) ) {
			return error_json(-32000,"Invalid characters in name",id);
		}
		_patchFile = nm;
		std::string err = SavePatch(nm);
		if ( err == "" ) {
			return ok_json(id);
		} else {
			return error_json(-32700,err.c_str(),id);
		}
	}
	if ( strcmp(method,"patch_load")==0 ) {
		cJSON *c_nm = cJSON_GetObjectItem(params,"name");
		if ( ! c_nm ) {
			return error_json(-32000,"Missing name argument",id);
		}
		char *nm = c_nm->valuestring;
		if ( has_invalid_char(nm) ) {
			return error_json(-32000,"Invalid characters in name",id);
		}
		_patchFile = nm;
		return LoadPatch(_patchFile,id);
	}
	if ( strcmp(method,"mmtt_increment") == 0 ) {
		return AdjustValue(params,id,1);
	}
	if ( strcmp(method,"mmtt_toggle") == 0 ) {
		return AdjustValue(params,id,0);
	}
	if ( strcmp(method,"mmtt_decrement") == 0 ) {
		return AdjustValue(params,id,-1);
	}
	if ( strcmp(method,"mmtt_set") == 0 ) {
		cJSON *c_value = cJSON_GetObjectItem(params,"value");
		if ( ! c_value ) {
			return error_json(-32000,"Missing value argument",id);
		}
		if ( c_value->type != cJSON_Number ) {
			return error_json(-32000,"Expecting number type in value argument to mmtt_set",id);
		}
		cJSON *c_name = cJSON_GetObjectItem(params,"name");
		if ( ! c_name ) {
			return error_json(-32000,"Missing name argument",id);
		}
		if ( c_name->type != cJSON_String ) {
			return error_json(-32000,"Expecting string type in name argument to mmtt_set",id);
		}
		std::string nm = std::string(c_name->valuestring);

		map<std::string, MmttValue*>::iterator it = mmtt_values.find(nm);
		if ( it == mmtt_values.end() ) {
			errstr = NosuchSnprintf("No Mmtt parameter with that name - %s",nm.c_str());
			return error_json(-32000,errstr.c_str(),id);
		}
		MmttValue* kv = it->second;
		double f = c_value->valuedouble;
		if ( f < 0.0 || f > 1.0 ) {
			errstr = NosuchSnprintf("Invalid Mmtt parameter - name=%s value=%lf - must be between 0 and 1",nm.c_str(),f);
			return error_json(-32000,errstr.c_str(),id);
		}
		kv->set_value(f);
		_updateValue(nm,kv);
		DEBUGPRINT(("mmtt_SET name=%s value=%lf\n",nm.c_str(),kv->value));
		return NosuchSnprintf(
			"{\"jsonrpc\": \"2.0\", \"result\": %lf, \"id\": \"%s\"}", kv->value, id);
	}
	if ( strcmp(method,"mmtt_get") == 0 ) {
		cJSON *c_name = cJSON_GetObjectItem(params,"name");
		if ( ! c_name ) {
			return error_json(-32000,"Missing name argument",id);
		}
		if ( c_name->type != cJSON_String ) {
			return error_json(-32000,"Expecting string type in name argument to mmtt_get",id);
		}
		std::string nm = std::string(c_name->valuestring);

		if ( nm == "patchname" ) {
			DEBUGPRINT(("mmtt_GET name=%s value=%s\n",nm.c_str(),_patchFile.c_str()));
			return NosuchSnprintf(
				"{\"jsonrpc\": \"2.0\", \"result\": \"%s\", \"id\": \"%s\"}", _patchFile.c_str(), id);
		}

		map<std::string, MmttValue*>::iterator it = mmtt_values.find(nm);
		if ( it == mmtt_values.end() ) {
			errstr = NosuchSnprintf("No Mmtt parameter with that name - %s",nm.c_str());
			return error_json(-32000,errstr.c_str(),id);
		}
		MmttValue* kv = it->second;
		DEBUGPRINT(("mmtt_GET name=%s value=%lf\n",nm.c_str(),kv->value));
		return NosuchSnprintf(
			"{\"jsonrpc\": \"2.0\", \"result\": %lf, \"id\": \"%s\"}", kv->value, id);
	}

	errstr = NosuchSnprintf("Unrecognized method name - %s",method);
	return error_json(-32000,errstr.c_str(),id);
}

void
MmttServer::doDepthRegistration()
{
	if ( autoDepth )
		doAutoDepthRegistration();
	else
		doManualDepthRegistration();

	DEBUGPRINT1(("After DepthRegistration, val_depth_front=%lf  depth_detect_top_mm=%lf  depth_detect_bottom_mm=%lf\n",
		val_depth_front.value,val_depth_detect_top.value,val_depth_detect_bottom.value));
}

void
MmttServer::doManualDepthRegistration()
{
}

void
MmttServer::doAutoDepthRegistration()
{
	DEBUGPRINT(("Starting AutoDepth registration!\n"));

	// Using depth_mm (millimeter depth values for the image), scan the image with a distance-window (100 mm?)
	// starting from the front, toward the back, looking for the first peak and then the first valley.
	int dwindow = (int)val_auto_window.value;
	int dinc = 10;
	int max_trigger = (int)( (camera->height() * camera->width() ) / 15);
	int min_trigger = (int)( (camera->height() * camera->width() ) / 60);
	int max_tot_sofar = 0;
	int mm_of_max_tot_sofar = 0;
	for (int mm = 1; mm < 3000; mm += dinc) {
		int tot = 0;
		int i = 0;
		int h = camera->height();
		int w = camera->width();
		for (int y=0; y<h; y++) {
			for (int x=0; x<w; x++) {
				int thismm = depthmm_front[i];
				if ( thismm >= mm && thismm < (mm+dwindow) ) {
					tot++;
				}
				i++;
			}
		}
		if ( tot > max_trigger && tot > max_tot_sofar ) {
				max_tot_sofar = tot;
				mm_of_max_tot_sofar = mm;
		}
		if ( max_tot_sofar > 0 && tot < min_trigger ) {
				break;
		}
	}
	val_depth_front.set_value(mm_of_max_tot_sofar);
	val_depth_detect_top.set_value(mm_of_max_tot_sofar + dwindow);   // + dwindow?
	val_depth_detect_bottom.set_value(val_depth_detect_top.value);

	return;
}

std::string
MmttServer::LoadPatchJson(std::string jstr)
{
	_regionsDefinedByPatch = false;

	DEBUGPRINT1(("LoadPatchJson start"));
	cJSON *json = cJSON_Parse(jstr.c_str());
	if ( ! json ) {
		DEBUGPRINT(("Unable to parse json in patch file!?  json= %s\n",jstr.c_str()));
		return "Unable to parse json in patch file";
	}
	for ( map<std::string, MmttValue*>::iterator vit=mmtt_values.begin(); vit != mmtt_values.end(); vit++ ) {
		std::string nm = vit->first;
		MmttValue* v = vit->second;
		DEBUGPRINT1(("Looking for nm=%s in json",nm.c_str()));
		cJSON *jval = cJSON_GetObjectItem(json,nm.c_str());
		if ( jval == NULL ) {
			if ( nm.find_first_of("show") != 0
				&& nm.find_first_of("blob_filter") != 0
				&& nm.find_first_of("confidence") != 0
				) {
				DEBUGPRINT(("No value for '%s' in patch file!?\n",nm.c_str()));
			}
			continue;
		}
		if ( jval->type != cJSON_Number ) {
			DEBUGPRINT(("The type of '%s' in patch file is wrong!?\n",nm.c_str()));
			continue;
		}
		if ( nm == "tilt" ) {
			DEBUGPRINT1(("ignoring tilt value in patch"));
			continue;
		}
		DEBUGPRINT1(("Patch file value for '%s' is '%lf'\n",nm.c_str(),jval->valuedouble));
		v->set_value(jval->valuedouble);
		_updateValue(nm,v);
	}

	cJSON *regionsval = cJSON_GetObjectItem(json,"regions");
	if ( regionsval ) {
		_regionsDefinedByPatch = true;
		// Use regions as defined in the patch
		if ( regionsval->type != cJSON_Array ) {
			return("The type of regions in patch file is wrong!?\n");
		}
		int nregions = cJSON_GetArraySize(regionsval);
		DEBUGPRINT1(("Mmtt nregions=%d",nregions));
		int regionid = (int)_new_regions.size();
		for ( int n=0; n<nregions; n++ ) {
			cJSON *rv = cJSON_GetArrayItem(regionsval,n);

			cJSON *c_first_sid = cJSON_GetObjectItem(rv,"first_sid");
			if ( c_first_sid == NULL ) { return("Missing first_sid value in patch file!?"); }
			if ( c_first_sid->type != cJSON_Number ) { return("The type of first_sid in patch file is wrong!?\n"); }

			cJSON *c_x = cJSON_GetObjectItem(rv,"x");
			if ( c_x == NULL ) { return("Missing x value in patch file!?"); }
			if ( c_x->type != cJSON_Number ) { return("The type of x in patch file is wrong!?\n"); }

			cJSON *c_y = cJSON_GetObjectItem(rv,"y");
			if ( c_y == NULL ) { return("Missing y value in patch file!?"); }
			if ( c_y->type != cJSON_Number ) { return("The type of y in patch file is wrong!?\n"); }

			cJSON *c_width = cJSON_GetObjectItem(rv,"width");
			if ( c_width == NULL ) { return("Missing width value in patch file!?"); }
			if ( c_width->type != cJSON_Number ) { return("The type of width in patch file is wrong!?\n"); }

			cJSON *c_height = cJSON_GetObjectItem(rv,"height");
			if ( c_height == NULL ) { return("Missing height value in patch file!?"); }
			if ( c_height->type != cJSON_Number ) { return("The type of height in patch file is wrong!?\n"); }

			if ( c_width->valueint > _camWidth ) {
				// This happens when a kinect-generated config (640x480) gets read when the camera is a creative (320x240)
				return("The width of a region is larger than the camera width!?");
			}

			// int x = _camWidth - c_x->valueint - c_width->valueint;
			int x = c_x->valueint;
			int y = c_y->valueint;
			if ( (x + c_width->valueint) > _camWidth ) {
				return("The x position + width of a region is larger than the camera width!?  Wrong config for this camera?");
			}
			if ( y >= _camHeight ) {
				return("The y position of a region is larger than the camera height!?  Wrong config for this camera?");
			}
			CvRect rect = cvRect(x, y, c_width->valueint, c_height->valueint);
			int first_sid = c_first_sid->valueint;
			_new_regions.push_back(new MmttRegion(regionid,first_sid,rect));
			DEBUGPRINT1(("LoadPatchJson regionid=%d first_sid=%d x,y=%d,%d width,height=%d,%d",
				regionid,first_sid,c_x->valueint,c_y->valueint,c_width->valueint,c_height->valueint));
			regionid++;
		}
	}
	return("");
}

std::string
MmttServer::LoadGlobalDefaults()
{
	// These are default values, which can be overridden by the config file.
	_jsonport = 4444;
	_do_sharedmem = false;
	_sharedmemname = "mmtt_outlines";
	_do_tuio = true;
	_do_errorpopup;
	_do_python = false;
	_do_initialalign = false;
	_patchFile = "";
	_cameraType = "kinect";
	_cameraIndex = 0;
	_tempDir = "c:/windows/temp";
	NosuchDebugToConsole = false;
	NosuchDebugToLog = true;
	NosuchDebugSetLogDirFile(MmttLogDir(),"mmtt.debug");

	// Config file can override those values
	ifstream f;
	std::string fname = _configpath;

	f.open(fname.c_str());
	if ( ! f.good() ) {
		return NosuchSnprintf("No config file: %s",fname.c_str());
	}
	DEBUGPRINT(("Loading config=%s\n",fname.c_str()));
	std::string line;
	std::string jstr;
	while ( getline(f,line) ) {
		// Delete anything after a # (i.e. comments)
		string::size_type pound = line.find("#");
		if ( pound != line.npos ) {
			line = line.substr(0,pound);
		}
		if ( line.find_last_not_of(" \t\n") == line.npos ) {
			DEBUGPRINT1(("Ignoring blank/comment line=%s",line.c_str()));
			continue;
		}
		jstr += line;
	}
	f.close();

	cJSON *json = cJSON_Parse(jstr.c_str());
	if ( ! json ) {
		return NosuchSnprintf("Unable to parse json in file: %s",fname.c_str());
	}
	LoadConfigDefaultsJson(json);

	return "";
}

static cJSON *
getNumber(cJSON *json,char *name)
{
	cJSON *j = cJSON_GetObjectItem(json,name);
	if ( j && j->type == cJSON_Number )
		return j;
	return NULL;
}

static cJSON *
getString(cJSON *json,char *name)
{
	cJSON *j = cJSON_GetObjectItem(json,name);
	if ( j && j->type == cJSON_String )
		return j;
	return NULL;
}

void
MmttServer::LoadConfigDefaultsJson(cJSON* json)
{
	cJSON *j;

	if ( (j=getNumber(json,"sharedmem")) != NULL ) {
		_do_sharedmem = (j->valueint != 0);
	}
	if ( (j=getNumber(json,"showfps")) != NULL ) {
		val_showfps.set_value( j->valueint != 0 );
	}
	if ( (j=getNumber(json,"flipx")) != NULL ) {
		val_flipx.set_value( j->valueint != 0 );
	}
	if ( (j=getNumber(json,"flipy")) != NULL ) {
		val_flipy.set_value( j->valueint != 0 );
	}
	if ( (j=getNumber(json,"showregionrects")) != NULL ) {
		val_showregionrects.set_value( j->valueint != 0 );
	}
	if ( (j=getNumber(json,"showregionhits")) != NULL ) {
		val_showregionhits.set_value( j->valueint != 0 );
	}
	if ( (j=getNumber(json,"showgreyscaledepth")) != NULL ) {
		val_showgreyscaledepth.set_value( j->valueint != 0 );
	}
	if ( (j=getString(json,"sharedmemname")) != NULL ) {
		_sharedmemname = std::string(j->valuestring);
	}
	if ( (j=getNumber(json,"errorpopup")) != NULL ) {
		_do_errorpopup = (j->valueint != 0);
	}
	if ( (j=getNumber(json,"tuio")) != NULL ) {
		_do_tuio = (j->valueint != 0);
	}
	if ( (j=getString(json,"tuio.25d.clientlist")) != NULL ) {
		_Tuio1_25_OscClientList = std::string(j->valuestring);
	}
	if ( (j=getString(json,"tuio.2d.clientlist")) != NULL ) {
		_Tuio1_2_OscClientList = std::string(j->valuestring);
	}
	// Tuio2 isn't really well supported - I tried (and did) implement
	// the blob outline aspect of Tuio2, but it doesn't seem practical
	// for large blobs, hence the introduction of the shared memory
	// mechanism which is MUCH MUCH MUCH faster (even for just cursors).
	if ( (j=getString(json,"tuio2.clientlist")) != NULL ) {
		_Tuio2_OscClientList = std::string(j->valuestring);
	}

	if ( (j=getNumber(json,"initialalign")) != NULL ) {
		_do_initialalign = (j->valueint != 0);
	}
	if ( (j=getNumber(json,"port")) != NULL ) {
		_jsonport = j->valueint;
	}
	if ( (j=getString(json,"camera")) != NULL ) {
		_cameraType = std::string(j->valuestring);
	}
	if ( (j=getNumber(json,"cameraindex")) != NULL ) {
		_cameraIndex = j->valueint;
	}
	if ( (j=getString(json,"patch")) != NULL ) {
		_patchFile = std::string(j->valuestring);
	}
	if ( (j=getString(json,"patchdir")) != NULL ) {
		DEBUGPRINT(("patchdir is no longer used"));
	}
	if ( (j=getString(json,"tempdir")) != NULL ) {
		_tempDir = std::string(j->valuestring);
	}
	if ( (j=getNumber(json,"debugtoconsole")) != NULL ) {
		NosuchDebugToConsole = j->valueint?TRUE:FALSE;
	}
	if ( (j=getNumber(json,"debugtolog")) != NULL ) {
		NosuchDebugToLog = j->valueint?TRUE:FALSE;
	}
	if ( (j=getNumber(json,"debugautoflush")) != NULL ) {
		NosuchDebugAutoFlush = j->valueint?TRUE:FALSE;
	}
	if ( (j=getString(json,"debuglogfile")) != NULL ) {
		NosuchDebugSetLogDirFile(MmttLogDir(),std::string(j->valuestring));
	}
	if ( (j=getNumber(json,"python")) != NULL ) {
		_do_python = j->valueint?TRUE:FALSE;
	}
}

// The default value of id is NULL, so the return value of LoadPatch is just a string.
// If id is non-NULL, then it means we want to return a JSON value.
std::string
MmttServer::LoadPatch(std::string patchname, const char *id)
{
	std::string err = LoadPatchReal(_patchFile);
	if ( err == "" ) {
		// Do it again?  There's a bug somewhere where it doesn't get completely initialized
		// unless you do it twice!?  HACK!!  Do not remove this unless you really test everything (including
		// registration on a new frame) to make sure everything still works.
		DEBUGPRINT(("Loading Patch a second time")); // HACK!!
		(void) LoadPatchReal(_patchFile);
		if ( id ) {
			return ok_json(id);
		} else {
			return "";
		}
	} else {
		if ( id ) {
			return error_json(-32000,err.c_str(),id);
		} else {
			return err;
		}
	}
}

std::string
MmttServer::LoadPatchReal(std::string patchname)
{
	startNewRegions();

	std::string fname = MmttPath("config/mmtt/" + patchname);
	if ( fname.find(".json") == fname.npos ) {
		fname = fname + ".json";
	}
	ifstream f;

	f.open(fname.c_str());
	if ( ! f.good() ) {
		std::string err = NosuchSnprintf("Warning - unable to open patch file: %s",fname.c_str());
		DEBUGPRINT(("%s",err.c_str()));  // avoid re-interpreting %'s and \\'s in name
		// If it's not in the main MmttPatchDir, try the tempdir, since
		// that's where new patches get saved
		fname = NosuchForwardSlash(_tempDir + "/" + patchname);
		DEBUGPRINT(("Trying tempdir, fname=%s",fname.c_str()));
		if ( fname.find(".json") == fname.npos ) {
			fname = fname + ".json";
		}
		f.open(fname.c_str());
		if ( ! f.good() ) {
			std::string err = NosuchSnprintf("Warning - unable to open patch file: %s",fname.c_str());
			DEBUGPRINT(("%s",err.c_str()));  // avoid re-interpreting %'s and \\'s in name
			return err;
		}
	}
	DEBUGPRINT(("Loading patch=%s\n",fname.c_str()));
	std::string line;
	std::string jstr;
	while ( getline(f,line) ) {
		DEBUGPRINT1(("patch line=%s\n",line.c_str()));
		if ( line.size()>0 && line.at(0)=='#' ) {
			DEBUGPRINT1(("Ignoring comment line=%s\n",line.c_str()));
			continue;
		}
		jstr += line;
	}
	f.close();
	std::string err = LoadPatchJson(jstr);
	if ( err != "" ) {
		return err;
	}

	if ( ! _regionsDefinedByPatch ) {

		std::string patchdir = MmttPath("config/mmtt");
		std::string fn_patch_image = patchdir + "/" + patchname + ".ppm";
		DEBUGPRINT(("Reading mask image from %s",fn_patch_image.c_str()));
		IplImage* img = cvLoadImage( fn_patch_image.c_str(), CV_LOAD_IMAGE_COLOR );
		if ( ! img ) {
			DEBUGPRINT(("Unable to open or read %s!?",fn_patch_image.c_str()));
			return NosuchSnprintf("Unable to read image file: %s",fn_patch_image.c_str());
		}
		copyColorImageToRegionsAndMask((unsigned char *)(img->imageData), _regionsImage, _maskImage, TRUE, TRUE);
		deriveRegionsFromImage();
	} else {
		DEBUGPRINT1(("Not reading mask image, since regions defined in patch"));
		copyRegionRectsToRegionsImage( _regionsImage, TRUE, TRUE);
	}

	finishNewRegions();

	if ( ! _regionsDefinedByPatch ) {
		// When we've used an image to define the regions, we resave the patch and then reload it, 
		// in order to get everything in sync.  Note that this writes the region values into the
		// .json file, so the image won't be used in future reloading of the patch.
		std::string err;
		err = SavePatch(patchname);
		if ( err != "" ) {
			return NosuchSnprintf("Unable to save patch: %s, err=%s",patchname.c_str(),err.c_str());
		}
		err = LoadPatch(patchname);
		if ( err != "" ) {
			return NosuchSnprintf("Unable to reload patch: %s, err=%s",patchname.c_str(),err.c_str());
		}
	}

	return "";
}

void MmttServer::startNewRegions()
{
	DEBUGPRINT1(("startNewRegions!!"));
	_new_regions.clear();
	CvRect all = cvRect(0,0,_camWidth,_camHeight);
	_new_regions.push_back(new MmttRegion(0,0,all));   // region 0 - the whole image?  Not really used, I think
	_new_regions.push_back(new MmttRegion(1,0,all));   // region 1 - the background
}

void MmttServer::finishNewRegions()
{
	size_t nrsize = _new_regions.size();
	size_t crsize = _curr_regions.size();
	if ( nrsize == crsize ) {
		DEBUGPRINT1(("finishNewRegions!! copying first_sid values"));
		for ( size_t i=0; i<nrsize; i++ ) {
			MmttRegion* cr = _curr_regions[i];
			MmttRegion* nr = _new_regions[i];
			nr->_first_sid = cr->_first_sid;
		}
	} else {
		if ( nrsize!=0 && crsize!=0 ) {
			DEBUGPRINT(("WARNING!! _curr_regions=%d _new_regions=%d size doesn't match!",crsize,nrsize));
		}
	}
	DEBUGPRINT1(("finishNewRegions!! _curr_regions follows"));
	_curr_regions = _new_regions;
	for ( size_t i=0; i<nrsize; i++ ) {
		MmttRegion* cr = _curr_regions[i];
		DEBUGPRINT1(("REGION i=%d xy=%d,%d wh=%d,%d",i,cr->_rect.x,cr->_rect.y,cr->_rect.width,cr->_rect.height));
	}
}

void
MmttServer::deriveRegionsFromImage()
{
	int region_id_minx[MAX_REGION_ID+1];
	int region_id_miny[MAX_REGION_ID+1];
	int region_id_maxx[MAX_REGION_ID+1];
	int region_id_maxy[MAX_REGION_ID+1];

	int id;
	for ( id=0; id<=MAX_REGION_ID; id++ ) {
		region_id_minx[id] = _camWidth;
		region_id_miny[id] = _camHeight;
		region_id_maxx[id] = -1;
		region_id_maxy[id] = -1;
	}

	for (int x=0; x<_camWidth; x++ ) {
		for (int y=0; y<_camHeight; y++ ) {
			id = _regionsImage->imageData[x+y*_camWidth];
			if ( id < 0 || id > MAX_REGION_ID ) {
				DEBUGPRINT(("Hey, regionsImage has byte > MAX_REGION_ID? (%d) at %d,%d\n",id,x,y));
				continue;
			}
			if ( x < region_id_minx[id] ) {
				region_id_minx[id] = x;
			}
			if ( y < region_id_miny[id] ) {
				region_id_miny[id] = y;
			}
			if ( x > region_id_maxx[id] ) {
				region_id_maxx[id] = x;
			}
			if ( y > region_id_maxy[id] ) {
				region_id_maxy[id] = y;
			}
		}
	}
	// The loop here doesn't include MAX_REGION_ID, because that's the mask id
	int first_sid = 1000;
	for ( id=2; id<MAX_REGION_ID; id++ ) {
		if ( region_id_maxx[id] == -1 || region_id_maxy[id] == -1 ) {
				continue;
		}
		DEBUGPRINT1(("Adding Region=%d  minxy=%d,%d maxxy=%d,%d\n",id-1,
			region_id_minx[id],
			region_id_miny[id],
			region_id_maxx[id],
			region_id_maxy[id]));

		// Need to flip the x values, for some reason
		int adjusted_minx = _camWidth - region_id_minx[id];
		int adjusted_maxx = _camWidth - region_id_maxx[id];
		if ( adjusted_minx > adjusted_maxx ) {
			int t = adjusted_minx;
			adjusted_minx = adjusted_maxx;
			adjusted_maxx = t;
		}

		int w = adjusted_maxx - adjusted_minx + 1;
		int h = region_id_maxy[id] - region_id_miny[id] + 1;
		_new_regions.push_back(new MmttRegion(id,first_sid,cvRect(adjusted_minx,region_id_miny[id],w,h)));
		DEBUGPRINT(("derived region id=%d x,y=%d,%d width,height=%d,%d",
				id,adjusted_minx,region_id_miny[id],w,h));
		first_sid += 1000;
	}
}

std::string
MmttServer::SavePatch(std::string patchname)
{
	if ( ! _regionsfilled ) {
		DEBUGPRINT(("Unable to save patch, regions are not filled yet"));
		return "Unable to save patch, regions are not filled yet";
	}
	std::string patchdir = MmttPath("config/mmtt");
	std::string fname = patchdir + "/" + patchname + ".json";
	ofstream f_json(fname.c_str());
	if ( ! f_json.is_open() ) {
		DEBUGPRINT(("Unable to open %s!?",fname.c_str()));
		// If we can't write in patchdir, try the tempdir
		patchdir = _tempDir;
		fname = NosuchForwardSlash(patchdir + "/" + patchname + ".json");
		DEBUGPRINT(("Trying tempdir, fname=%s",fname.c_str()));
		f_json.open(fname.c_str());
		if ( ! f_json.is_open() ) {
			DEBUGPRINT(("Unable to open %s!?",fname.c_str()));
			return "Unable to open patch file";
		}
	}
	f_json << "{";

	std::string sep = "\n";
	for ( map<std::string, MmttValue*>::iterator vit=mmtt_values.begin(); vit != mmtt_values.end(); vit++ ) {
		std::string nm = vit->first;
		MmttValue* v = vit->second;
		f_json << sep << "  \"" << nm << "\": " << v->value;
		sep = ",\n";
	}
	f_json << sep << "  \"regions\": [";
	std::string sep2 = "\n";
	for ( vector<MmttRegion*>::iterator ait=_curr_regions.begin(); ait != _curr_regions.end(); ait++ ) {
		MmttRegion* r = *ait;
		// internal id 0 isn't exposed
		if ( r->_first_sid == 0 ) {
			continue;
		}
		f_json << sep2 << "    { \"first_sid\": " << (r->_first_sid)
			<< ", \"x\": " << r->_rect.x
			<< ", \"y\": " << r->_rect.y
			<< ", \"width\": " << r->_rect.width
			<< ", \"height\": " << r->_rect.height
			<< "}";
		sep2 = ",\n";
	}
	f_json << "\n  ]\n";
	f_json << "\n}\n";
	f_json.close();
	DEBUGPRINT(("Saved patch: %s",fname.c_str()));

	std::string fn_patch_image = patchdir + "/" + patchname + ".ppm";

	copyRegionsToColorImage(_regionsImage,(unsigned char *)(_tmpRegionsColor->imageData),TRUE,TRUE,TRUE);
	if ( !cvSaveImage(fn_patch_image.c_str(),_tmpRegionsColor) ) {
		DEBUGPRINT(("Could not save ppm: %s\n",fn_patch_image.c_str()));
	} else {
		DEBUGPRINT(("Saved ppm: %s",fn_patch_image.c_str()));
	}
	return "";
}

void
MmttServer::registrationStart()
{
	CvConnectedComp connected;

	// Grab the Mask
	cvCvtColor( _ffImage, _tmpGray, CV_BGR2GRAY );
	if ( doSmooth ) {
		cvSmooth( _tmpGray, _tmpGray, CV_GAUSSIAN, 9, 9);
	}
	cvThreshold( _tmpGray, _maskImage, val_blob_param1.value, 255, CV_THRESH_BINARY );
	cvDilate( _maskImage, _maskImage, NULL, MaskDilateAmount );
	DEBUGPRINT1(("GRABMASK has been done!"));
	cvCopy(_maskImage,_regionsImage);
	DEBUGPRINT1(("GRABMASK copied to Regions"));
	// crosshairPoint = cvPoint(_camWidth / 2, _camHeight / 2);

	// Go along all the outer edges of the image and fill any black (i.e. background)
	// pixels with the first region color. 
	int x;
	int y = 0;
	for (x=0; x<_camWidth; x++ ) {
		if ( _regionsImage->imageData[x+y*_camWidth] == 0 ) {
			cvFloodFill(_regionsImage, cvPoint(x,y), cvScalar(currentRegionValue), cvScalarAll(0), cvScalarAll(0), &connected);
		}
	}
	y = _camHeight-1;
	for (x=0; x<_camWidth; x++ ) {
		if ( _regionsImage->imageData[x+y*_camWidth] == 0 ) {
			cvFloodFill(_regionsImage, cvPoint(x,y), cvScalar(currentRegionValue), cvScalarAll(0), cvScalarAll(0), &connected);
		}
	}
	x = 0;
	for (int y=0; y<_camHeight; y++ ) {
		if ( _regionsImage->imageData[x+y*_camWidth] == 0 ) {
			cvFloodFill(_regionsImage, cvPoint(x,y), cvScalar(currentRegionValue), cvScalarAll(0), cvScalarAll(0), &connected);
		}
	}
	x = _camWidth-1;
	for (int y=0; y<_camHeight; y++ ) {
		if ( _regionsImage->imageData[x+y*_camWidth] == 0 ) {
			cvFloodFill(_regionsImage, cvPoint(x,y), cvScalar(currentRegionValue), cvScalarAll(0), cvScalarAll(0), &connected);
		}
	}

	startNewRegions();
	currentRegionValue = 2;
}

void
MmttServer::doRegistration()
{
	DEBUGPRINT1(("doRegistration state=%d",registrationState));

	if ( registrationState == 100 ) {
		currentRegionValue = 1;
		val_showregionmap.set_value(1.0);
		// Figure out the depth of the Mask
		doDepthRegistration();
		registrationState = 110;  // Start registering right away, but wait a couple frames
		return;
	}

	if ( registrationState >= 110 && registrationState < 120 ) {
		registrationState++;
		if ( registrationState == 113 ) {
			// Continue on to the manual registration process
			registrationState = 120;
		}
		return;
	}

	if ( registrationState == 120 ) {
		DEBUGPRINT1(("Starting registration!\n"));
		registrationStart();
		// Continue the manual registration process (wait for someone
		// to poke their hand in each region
		registrationState = 199;
		return;
	}

	if ( registrationState == 199 ) {
		registrationContinueManual();
		return;
	}

	if ( registrationState == 300 ) {
		DEBUGPRINT(("State 300"));
		// Start auto-poke.  First re-do the depth registration,
		// then start poking the center of each region
		currentRegionValue = 1;
		doDepthRegistration();   // try without, for new file-based autopoke
		if ( continuousAlign ) {
			DEBUGPRINT(("registration state 300"));
			copyRegionsToColorImage(_regionsImage,_ffpixels,FALSE,FALSE,FALSE);
		}
		registrationState = 310;
		return;
	}

	if ( registrationState >= 310 && registrationState < 320) {
		registrationState++;
		if ( registrationState == 312 ) {
			registrationState = 320;
		}
		if ( continuousAlign ) {
			copyRegionsToColorImage(_regionsImage,_ffpixels,FALSE,FALSE,FALSE);
		}
		return;
	}

	if ( registrationState == 320 ) {
		DEBUGPRINT1(("State 320"));
		val_showregionmap.set_value(1.0);

		_savedpokes.clear();

		ifstream autopokefile;
		autopokefile.open("autopoke.txt");
		if ( autopokefile.good() ) {
			std::string line;
			// while ( autopokefile.getline(line,sizeof(line)) ) {
			while ( getline(autopokefile,line) ) {
				DEBUGPRINT(("autopoke input line=%s\n",line.c_str()));
				std::stringstream ss;
				CvPoint pt;
				ss << line;
				ss >> pt.x;
				ss >> pt.y;
				_savedpokes.push_back(pt);
				DEBUGPRINT(("    pt.x=%d y=%d\n",pt.x,pt.y));
			}
			autopokefile.close();
		} else {
			std::vector<MmttRegion*>::const_iterator it;
			for ( it=_curr_regions.begin(); it!=_curr_regions.end(); it++ ) {
					MmttRegion* r = (MmttRegion*)(*it);
					if ( r->_first_sid <= 1 ) {
						// The first two regions are the background and mask
						continue;
					}
					CvRect rect = r->_rect;
					CvPoint pt = cvPoint(rect.x+rect.width/2,rect.y+rect.height/2);
					DEBUGPRINT1(("Region id=%d pt=%d,%d\n",r->_first_sid - 1,pt.x,pt.y));
					_savedpokes.push_back(pt);
			}
		}
		registrationStart();
		DEBUGPRINT1(("after registrationStart, going to 330"));
		registrationState = 330;
		if ( continuousAlign ) {
			copyRegionsToColorImage(_regionsImage,_ffpixels,FALSE,FALSE,FALSE);
		}
		return;
	}

	if ( registrationState == 330 ) {
		DEBUGPRINT1(("State 330, calling doPokeRegistration"));
		doPokeRegistration();
		registrationState = 340;
		if ( continuousAlign ) {
			copyRegionsToColorImage(_regionsImage,_ffpixels,FALSE,FALSE,FALSE);
		}
		return;
	}

	if ( registrationState >= 340 && registrationState < 360 ) {
		registrationState++;
		if ( registrationState == 359 || (continuousAlign && registrationState == 343) ) {
			bool stopit = true;
			if ( continuousAlign ) {
				stopit = false;
#ifdef THIS_SHOULD_NOT_BE_DONE
				std::string err = LoadPatch(_patchFile);
				if ( err != "" ) {
					DEBUGPRINT(("LoadPatch in continuousAlign failed!?  err=%s",err.c_str()));
				}
#endif
				// int needed_regions = currentRegionValue;
				size_t needed_regions = _curr_regions.size();
				if ( _new_regions.size() == needed_regions ) {
					DEBUGPRINT(("Continuous registration, OKAY (_new_regions.size=%d needed_regions=%d continuousAlignOkayCount=%d)",_new_regions.size(),needed_regions,continuousAlignOkayCount));
					continuousAlignOkayCount++;
					if ( continuousAlignOkayCount > continuousAlignOkayEnough ) {
						// We've had enough Okay registrations, so stop the continuousAlign registration
						DEBUGPRINT(("STOPPING continuousAlign registration, continuousAlignOkayCount=%d",continuousAlignOkayCount));
						continuousAlign = false;
						stopit = true;
					}
				} else {
					DEBUGPRINT(("Continuous registration, NOT OKAY (missing %d regions)",needed_regions - _new_regions.size()));
					continuousAlignOkayCount = 0;
				}
			}
			if ( stopit ) {
				_stop_registration();
			} else {
				registrationState = 300;  // restart registration
			}
		}
		copyRegionsToColorImage(_regionsImage,_ffpixels,FALSE,FALSE,FALSE);
		return;
	}

	DEBUGPRINT(("Hey!  Unexpected registrationState: %d\n",registrationState));
	return;

}

void
MmttServer::doPokeRegistration()
{
	DEBUGPRINT(("doPokeRegistration"));
	// check _savedpokes
	std::vector<CvPoint>::const_iterator it;
	for ( it=_savedpokes.begin(); it!=_savedpokes.end(); it++ ) {
			CvPoint pt = (CvPoint)(*it);
			registrationPoke(pt);
	}
	copyRegionsToColorImage(_regionsImage,_ffpixels,FALSE,FALSE,FALSE);
}

void
MmttServer::registrationContinueManual()
{
	removeMaskFrom(thresh_front);

	_tmpThresh->origin = 1;
	_tmpThresh->imageData = (char *)thresh_front;

	CBlobResult blobs = CBlobResult(_tmpThresh, NULL, 0);

	blobs.Filter( blobs, B_EXCLUDE, CBlobGetArea(), B_GREATER, val_blob_maxsize.value );
	blobs.Filter( blobs, B_EXCLUDE, CBlobGetArea(), B_LESS, val_blob_minsize.value );

	CvRect bigRect = cvRect(0,0,0,0);
	int bigarea = 0;
	for ( int i=0; i<blobs.GetNumBlobs(); i++ ) {
		CBlob *b = blobs.GetBlob(i);
		CvRect r = b->GetBoundingBox();

		CvPoint pt = cvPoint(r.x+r.width/2,r.y+r.height/2);
		// If the middle of the blob is already an assigned region (or the frame), ignore it
		unsigned char v = _regionsImage->imageData[pt.x+pt.y*_camWidth];

		// This hack for auto-stopping the registration process only works if there's more than one region to set
		if ( v == 0 || v < (currentRegionValue-1) ) {
			int area = r.width*r.height;
			// For crosshair detection, min blob size is greater than normal
			if ( area > bigarea && area > (5*val_blob_minsize.value) ) {
				bigRect = r;
				bigarea = area;
			}
		}
	}
	if ( bigarea != 0 ) {
		registrationPoke(cvPoint(bigRect.x + bigRect.width/2, bigRect.y + bigRect.height/2));
	}

	// SHOW ANY EXISTING AREAS
	copyRegionsToColorImage(_regionsImage,_ffpixels,FALSE,FALSE,FALSE);
	return;
}

void
MmttServer::registrationPoke(CvPoint pt)
{
	// If this point isn't already set to an existing region (and isn't the frame),
	// make it a new region
	unsigned char v = _regionsImage->imageData[pt.x+pt.y*_camWidth];
	if ( v == 0 ) {
		// If the point is black, we've got a new region
		CvConnectedComp connected;
		cvFloodFill(_regionsImage, pt, cvScalar(currentRegionValue), cvScalarAll(0), cvScalarAll(0), &connected);
		int first_sid = (currentRegionValue-1) * 1000;
		_new_regions.push_back(new MmttRegion(currentRegionValue,first_sid,connected.rect));
		CvRect r = connected.rect;
		DEBUGPRINT1(("Creating new region %d at minxy=%d,%d maxxy=%d,%d\n",currentRegionValue-1,r.x,r.y,r.x+r.width,r.y+r.height));
		currentRegionValue++;
	} else if ( v == 2 ) {
		DEBUGPRINT1(("Repeated first region - registration is terminated"));
		_stop_registration();
	} else if ( v == 1 ) {
		// If the point is red (the background), we haven't got a new region
		DEBUGPRINT1(("v==1 for registrationPoke pt=%d,%d",pt.x,pt.y));
	}
}

int
MmttRegion::getAvailableSid() {
	int available = 0;
	// This assumes that the map iteration goes in ascending order of session ids (which are the key).
	// We find the lowest session id that doesn't already exist.
	for ( map<int,MmttSession*>::iterator it = _sessions.begin(); it != _sessions.end(); it++ ) {
		int sid = (*it).first;
		if ( sid != available ) {
			return available;
		}
		available++;
	}
	return available;
}

void
MmttServer::removeMaskFrom(uint8_t* pixels)
{
	unsigned char *maskdata = (unsigned char *)_maskImage->imageData;
	int i = 0;
	for (int x=0; x<_camWidth; x++ ) {
		for (int y=0; y<_camHeight; y++ ) {
			unsigned char g = maskdata[i];
			if ( g != 0 ) {
				pixels[i] = 0;
			}
			i++;
		}
	}
}

void
MmttServer::analyzePixels()
{
	if ( registrationState > 0 ) {
		doRegistration();
		return;
	}

	if ( doSmooth ) {
		cvSmooth( _tmpGray, _tmpGray, CV_GAUSSIAN, 9, 9);
	}

	if ( val_usemask.value ) {
		removeMaskFrom(thresh_front);
	}

	_tmpThresh->origin = 1;
	_tmpThresh->imageData = (char *)thresh_front;
	_newblobresult = new CBlobResult(_tmpThresh, NULL, 0);

	if ( val_blob_filter.value != 0.0 ) {
		// If the value of blob_maxsize is 1.0 (the maximum external value), turn off max filtering
		if ( val_blob_maxsize.value < val_blob_maxsize.maxvalue ) {
			_newblobresult->Filter( *_newblobresult, B_EXCLUDE, CBlobGetArea(), B_GREATER, val_blob_maxsize.value );
		}
		_newblobresult->Filter( *_newblobresult, B_EXCLUDE, CBlobGetArea(), B_LESS, val_blob_minsize.value );
	}

	int numblobs = _newblobresult->GetNumBlobs();

	// XXX - should really make these static/global so they don't get re-allocated every time, just clear them
	// XXX - BUT, make sure they get re-initialized (e.g. blob_sid's should start out as -1).
	std::vector<MmttRegion*> blob_region(numblobs,NULL);
	std::vector<CvPoint> blob_center(numblobs);
	std::vector<int> blob_sid(numblobs,-1);
	std::vector<CvRect> blob_rect(numblobs);

	// Go through the blobs and identify the region each is in

	_framenum++;
	_fpscount++;

	size_t nregions = _curr_regions.size();
	for ( int i=0; i<numblobs; i++ ) {
		CBlob *blob = _newblobresult->GetBlob(i);
		CvRect r = blob->GetBoundingBox();   // this is expensive, I think
		blob_rect[i] = r;
		CvPoint center = cvPoint(r.x + r.width/2, r.y + r.height/2);
		blob_center[i] = center;
		unsigned char g = _regionsImage->imageData[center.x+center.y*_camWidth];
		// 0 is the background color
		// 1 is the "filled in" background (starting from the edges)
		if ( g != 0 ) {
			DEBUGPRINT1(("blob num = %d  g=%d  area=%.3f",i,g,blob->Area()));
		}
		if ( g > 1 && g < MASK_REGION_ID ) {
			if ( g >= nregions ) {
				DEBUGPRINT(("Hey, g (%d) is greater than number of regions (%d)\n",g,nregions));
			} else {
				blob_region[i] = _curr_regions[g];
			}
		}
	}

	// For each region...
	for ( vector<MmttRegion*>::iterator ait=_curr_regions.begin(); ait != _curr_regions.end(); ait++ ) {
		MmttRegion* r = *ait;

		// Scan existing sessions for the region
		for ( map<int,MmttSession*>::iterator it = r->_sessions.begin(); it != r->_sessions.end(); ) {

			int sid = (*it).first;
			MmttSession* sess = (*it).second;

			double mindist = 999999.0;
			int mindist_i = -1;

			// Find the closest blob to the session's center
			for ( int i=0; i<numblobs; i++ ) {
				if ( blob_region[i] != r ) {
					// blob isn't in the region we're looking at
					continue;
				}
				if ( blob_sid[i] >= 0 ) {
					// blob has already been assigned to a session
					continue;
				}
				CvPoint blobcenter = blob_center[i];
				int dx = abs(blobcenter.x - sess->_center.x);
				int dy = abs(blobcenter.y - sess->_center.y);
				double dist = sqrt(double(dx*dx+dy*dy));
				if ( dist < mindist ) {
					mindist = dist;
					mindist_i = i;
				}
			}
			if ( mindist_i >= 0 ) {
				// Update the session with the new blob
				CBlob *blob = _newblobresult->GetBlob(mindist_i);

				sess->_blob = blob;
				sess->_center = blob_center[mindist_i];

				// r->_sessions[sid] = new MmttSession(blob,sess->_frame_born);

				blob_sid[mindist_i] = sid;
				it++;
			} else {
				// No blob found for this session, remove session
				map<int,MmttSession*>::iterator erase_it = it;
				it++;

				delete sess;
				r->_sessions.erase(erase_it);
			}
		}
	}

	// Go back through the blobs. Any that are not attached to an existing session id
	// will be attached to a new session.  This is also where we compute
	// the depth of each blob/session.

	int nactive = 0;
	bool didtitle = FALSE;
	for ( int i=0; i<numblobs; i++ ) {
		MmttRegion* r = blob_region[i];
		if ( r == NULL ) {
			continue;
		}
		CBlob *blob = _newblobresult->GetBlob(i);

		// Go through the blob and get average depth
		float depthtotal = 0.0f;
		float depth_adjusted_total = 0.0f;
		int depthcount = 0;
		CvRect blobrect = blob->GetBoundingBox();
		int endy = blobrect.y + blobrect.height;

		// XXX - THIS CODE NEEDS Optimization (probably)

		for ( int y=blobrect.y; y<endy; y++ ) {
			int yi = y * _camWidth + blobrect.x;

			float backval = (float)(val_depth_detect_top.value
				+ (val_depth_detect_bottom.value - val_depth_detect_top.value)
				* (float(y)/_camHeight));

			for ( int dx=0; dx<blobrect.width; dx++ ) {
				int mm = depthmm_front[yi+dx];
				if ( mm == 0 || mm < val_depth_front.value || mm > backval )
					continue;
				depthtotal += mm;
				depth_adjusted_total += (backval-mm);
				depthcount++;
			}
		}
		if ( depthcount == 0 ) {
			continue;
		}

		int sid = blob_sid[i];
		if ( sid < 0 ) {
			// New session!
			int new_sid = r->getAvailableSid();
			DEBUGPRINT1(("New Session new_sid=%d!",new_sid));
			r->_sessions[new_sid] = new MmttSession(blob,blob_center[i],_framenum);
			blob_sid[i] = new_sid;
			sid = new_sid;
		}

		float depthavg = depthtotal / depthcount;
		float depth_adjusted_avg = depth_adjusted_total / depthcount;
		r->_sessions[sid]->_depth_mm = (int)(depthavg + 0.5f);
		r->_sessions[sid]->_depth_normalized = depth_adjusted_avg / 1000.0f;

		if ( depthavg > 0 ) {
			double ar = blobrect.width*blobrect.height;
			DEBUGPRINT1(("BLOB sid=%d area=%d  depth count=%d avg=%d adjusted_avg=%d\n",sid,ar,depthcount,depthavg,depth_adjusted_avg));
			if ( ar > 0.0 ) {
				CBlobContour *contour = blob->GetExternalContour();
				CvSeq* points = contour->GetContourPoints();
				DEBUGPRINT2(("Blob i=%d contour=%d points->total=%d",i,(int)contour,points->total));
				CvPoint pt0;
				for(int i = 0; i < points->total; i++)
				{
					pt0 = *CV_GET_SEQ_ELEM( CvPoint, points, i );
					DEBUGPRINT2(("i=%d pt=%d,%d",i,pt0.x,pt0.y));
				}
			}
		}
		nactive++;
	}

	if ( _do_tuio ) {
		if ( _Tuio1_25_Clients.size() > 0 ) {
			doTuio1_25D(nactive,numblobs,blob_sid,blob_region,blob_center);
		}
		if ( _Tuio1_2_Clients.size() > 0 ) {
			doTuio1_2D(nactive,numblobs,blob_sid,blob_region,blob_center);
		}
		if ( _Tuio2_Clients.size() > 0 ) {
			doTuio2(nactive,numblobs,blob_sid,blob_region,blob_center);
		}
	}

	if ( _do_sharedmem ) {
		shmem_lock_and_update_outlines(nactive,numblobs,blob_sid,blob_region,blob_center);
	}

	if ( _oldblobresult ) {
		delete _oldblobresult;
	}
	_oldblobresult = _newblobresult;
	_newblobresult = NULL;

}

void
MmttServer::showBlobSessions()
{
	for ( vector<MmttRegion*>::iterator ait=_curr_regions.begin(); ait != _curr_regions.end(); ait++ ) {
		MmttRegion* r = *ait;
		for ( map<int,MmttSession*>::iterator it = r->_sessions.begin(); it != r->_sessions.end(); it++ ) {
			int sid = (*it).first;
			MmttSession* sess = r->_sessions[sid];

			// Sessions for which blob is NULL are created by the Leap
			if ( sess->_blob ) {
				CvRect blobrect = sess->_blob->GetBoundingBox();
				CvScalar c = colorOfSession(sid);
				int thick = 2;
				cvRectangle(_ffImage, cvPoint(blobrect.x,blobrect.y),
					cvPoint(blobrect.x+blobrect.width-1,blobrect.y+blobrect.height-1),
					c,thick,8,0);
			}
		}
	}
}

void
MmttServer::showRegionHits()
{
	std::vector<MmttRegion*>::const_iterator it;
	for ( it=_curr_regions.begin(); it != _curr_regions.end(); it++ ) {
		MmttRegion* r = (MmttRegion*) *it;
		if ( r->_sessions.size() > 0 ) {
			CvRect arect = r->_rect;
			CvScalar c = CV_RGB(128,128,128);
			c = region2cvscalar[r->id];
			DEBUGPRINT1(("showRegionHits id=%d c=0x%x",r->id,c));
			int thick = 1;
			cvRectangle(_ffImage, cvPoint(arect.x,arect.y), cvPoint(arect.x+arect.width-1,arect.y+arect.height-1), c,thick,8,0);
		}
	}
}

void
MmttServer::showRegionRects()
{
	std::vector<MmttRegion*>::const_iterator it;
	size_t nregions = _curr_regions.size();
	for ( size_t n=2; n<nregions; n++ ) {
		MmttRegion* r = _curr_regions[n];
		CvRect arect = r->_rect;
		CvScalar c = region2cvscalar[r->id];
		int thick = 1;
		cvRectangle(_ffImage, cvPoint(arect.x,arect.y),
			cvPoint(arect.x+arect.width-1,arect.y+arect.height-1), c,thick,8,0);
	}
}

void
MmttServer::showMask()
{
	for (int x=0; x<_camWidth; x++ ) {
		for (int y=0; y<_camHeight; y++ ) {
			int i = x + y*_camWidth;
			unsigned char g = _maskImage->imageData[i];
			_ffpixels[i*3 + 0] = g;
			_ffpixels[i*3 + 1] = g;
			_ffpixels[i*3 + 2] = g;
		}
	}
}

static void
write_span( CvPoint spanpt0, CvPoint spanpt1, OscMessage& msg, CvRect regionrect)
{
	if ( spanpt0.x > spanpt1.x ) {
		int tmp = spanpt0.x;
		spanpt0.x = spanpt1.x;
		spanpt1.x = tmp;
	}
	int spandx = spanpt1.x - spanpt0.x;
	int spandy = spanpt1.y - spanpt0.y;
	int dx = spanpt0.x - regionrect.x;
	int dy = spanpt0.y - regionrect.y;
	if ( spandy != 0 ) {
		DEBUGPRINT(("Hey, something is wrong, y of spanpt0 and spanpt1 should be the same!"));
		return;
	}
	float x = float(dx);
	float y = float(dy);
	normalize_region_xy(x,y,regionrect);

	float scaledlength = float(spandx+1) / (regionrect.width);
	if ( scaledlength > 1.0 ) {
		scaledlength = 1.0;
	}

	msg.addFloatArg(x);
	msg.addFloatArg(y);
	msg.addFloatArg(scaledlength);
}

void
MmttServer::doTuio2( int nactive, int numblobs, std::vector<int> &blob_sid, std::vector<MmttRegion*> &blob_region, std::vector<CvPoint> &blob_center) {
	OscBundle bundle;
	OscMessage msg;

	long tm = timeGetTime();
	if ( nactive == 0 ) {
		if ( (tm - _tuio_last_sent) > 100 ) {
			msg.clear();
			msg.setAddress("/tuio/25Dblb");
			msg.addStringArg("alive");
			bundle.addMessage(msg);
			msg.clear();
			msg.setAddress("/tuio/25Dblb");
			msg.addStringArg("fseq");
			msg.addIntArg(_tuio_fseq++);
			bundle.addMessage(msg);
			SendAllOscClients(bundle,_Tuio2_Clients);
			SendOscToAllWebSocketClients(bundle);
			_tuio_last_sent = tm;
		}
	} else {
		// Put out the TUIO messages

		bundle.clear();

		// First the "alive" message that lists all the active sessions
		msg.clear();
		msg.setAddress("/tuio/25Dblb");
		msg.addStringArg("alive");
		for ( int i=0; i<numblobs; i++ ) {
			MmttRegion* r = blob_region[i];
			int sid = blob_sid[i];
			if ( r == NULL || sid < 0 ) {
				continue;
			}
			CBlob *blob = _newblobresult->GetBlob(i);
			int tuio_sid = tuioSessionId(r,sid);
			msg.addIntArg(tuio_sid);
		}
		bundle.addMessage(msg);

		for ( int i=0; i<numblobs; i++ ) {
			MmttRegion* r = blob_region[i];
			int sid = blob_sid[i];
			if ( r == NULL || sid < 0 ) {
				continue;
			}
			CBlob *blob = _newblobresult->GetBlob(i);
			CvRect blobrect = blob->GetBoundingBox();

			CvRect regionrect = r->_rect;
			CvPoint blobcenter = blob_center[i];
			int dx = blobcenter.x - regionrect.x;
			int dy = blobcenter.y - regionrect.y;

			float x = float(dx);
			float y = float(dy);
			normalize_region_xy(x,y,regionrect);

			int tuio_sid = tuioSessionId(r,sid);

			MmttSession* sess = r->_sessions[sid];
			// float depth = (float)(val_depth_detect_top.value - sess->_depth_mm) / (float)val_depth_detect_top.value;
			float depth = (float)sess->_depth_normalized;
			float f = (float)(blobrect.width*blobrect.height) / (float)(regionrect.width*regionrect.height);

			msg.clear();
			msg.setAddress("/tuio/25Dblb");
			msg.addStringArg("set");
			msg.addIntArg(tuio_sid);
			msg.addFloatArg(x);      // x (position)
			msg.addFloatArg(y);      // y (position)
			msg.addFloatArg(depth);        // z (position)
			msg.addFloatArg(0.0);          // a (angle)
			msg.addFloatArg((float)blobrect.width);   // w (width)
			msg.addFloatArg((float)blobrect.height);  // h (height)
			msg.addFloatArg(f);			   // f (area)
			msg.addFloatArg(0.0);          // X (velocity)
			msg.addFloatArg(0.0);          // Y (velocity)
			msg.addFloatArg(0.0);          // Z (velocity)
			msg.addFloatArg(0.0);          // A (rotational velocity)
			msg.addFloatArg(0.0);          // m (motion acceleration)
			msg.addFloatArg(0.0);          // r (rotation acceleration)
			bundle.addMessage(msg);

			// CBlob *blob = _newblobresult->GetBlob(i);
			CBlobContour* contour = blob->GetExternalContour();
			t_chainCodeList contourpoints = contour->GetContourPoints();

			// debug message
			msg.clear();
			msg.setAddress("/tuio2/contourpoints");
			msg.addIntArg(contourpoints->total);
			bundle.addMessage(msg);

			if ( contourpoints->total > 0 ) {

#if 0
				// It's silly to send so much data in a single OSC message, for large blobs
				msg.clear();
				msg.setAddress("/tuio2/arg");
				msg.addIntArg(tuio_sid);

				CvPoint spanpt0 = *CV_GET_SEQ_ELEM( CvPoint, contourpoints, i );
				CvPoint spanpt1 = spanpt0;
				DEBUGPRINT2(("Starting /tuio2/arg message first pt0=%d,%d",spanpt0.x,spanpt0.y));
				CvPoint pt0;
				for(int i = 1; i < contourpoints->total; i++) {
					pt0 = *CV_GET_SEQ_ELEM( CvPoint, contourpoints, i );
					DEBUGPRINT2(("    next pt0=%d,%d",pt0.x,pt0.y));

					if ( pt0.y != spanpt0.y ) {

						// The point moved diagonally or up or down, put out the span as it currently exists
						write_span(spanpt0,spanpt1,msg,regionrect);

						spanpt0 = pt0;
						spanpt1 = pt0;

					} else if ( pt0.x != spanpt0.x ) {
						// point moved left or right
						spanpt1 = pt0;
					} else {
						DEBUGPRINT2(("Hey! contour point didn't move!?  i=%d pt0=%d,%d  spanpt0=%d,%d  spanpt1=%d,%d",i,pt0.x,pt0.y,spanpt0.x,spanpt0.y,spanpt1.x,spanpt1.y));
						// ignore, this happens occasionally, perhaps for really small features
						// of a contour that backtrack, or something
					}
				}
				// put out the final span
				write_span(spanpt0,spanpt1,msg,regionrect);
				DEBUGPRINT2(("Ending /tuio2/arg message"));

				bundle.addMessage(msg);
#endif
			}
		}

		msg.clear();
		msg.setAddress("/tuio/25Dblb");
		msg.addStringArg("fseq");
		msg.addIntArg(_tuio_fseq++);
		bundle.addMessage(msg);

		// The TUIO 2.0 support for blob outlines was implemented, but I found that
		// it sent too much data to be practical.
		// SendAllOscClients(bundle,_Tuio2_Clients);
	}
}

double
bound_it(double v) {
	if ( v < 0.0 ) {
		return 0.0;
	}
	if ( v > 1.0 ) {
		return 1.0;
	}
	return v;
}

bool
point_is_in(CvPoint pt, CvRect& rect) {
	if ( pt.x < rect.x ) return false;
	if ( pt.x > (rect.x+rect.width) ) return false;
	if ( pt.y < rect.y ) return false;
	if ( pt.y > (rect.y+rect.height) ) return false;
	return true;
}

int
make_hfid(int hid, int fid)
{
	return hid * 100000 + fid;
}

void
MmttServer::doTuio1_25D( int nactive, int numblobs, std::vector<int> &blob_sid, std::vector<MmttRegion*> &blob_region, std::vector<CvPoint> &blob_center) {
	OscBundle bundle;
	OscMessage msg;

	unsigned long tm = timeGetTime();
	if ( nactive == 0 ) {
		if ( (tm - _tuio_last_sent) > 100 ) {
			msg.clear();
			msg.setAddress("/tuio/25Dblb");
			msg.addStringArg("alive");
			bundle.addMessage(msg);
			msg.clear();
			msg.setAddress("/tuio/25Dblb");
			msg.addStringArg("fseq");
			msg.addIntArg(_tuio_fseq++);
			bundle.addMessage(msg);
			SendAllOscClients(bundle,_Tuio1_25_Clients);
			SendOscToAllWebSocketClients(bundle);
			_tuio_last_sent = tm;
		}
	} else {
		// Put out the TUIO messages

		bundle.clear();

		// First the "alive" message that lists all the active sessions
		msg.clear();
		msg.setAddress("/tuio/25Dblb");
		msg.addStringArg("alive");
		for ( int i=0; i<numblobs; i++ ) {
			MmttRegion* r = blob_region[i];
			int sid = blob_sid[i];
			if ( r == NULL || sid < 0 ) {
				continue;
			}
			CBlob *blob = _newblobresult->GetBlob(i);
			int tuio_sid = tuioSessionId(r,sid);
			msg.addIntArg(tuio_sid);
		}
		bundle.addMessage(msg);

		for ( int i=0; i<numblobs; i++ ) {
			MmttRegion* r = blob_region[i];
			int sid = blob_sid[i];
			if ( r == NULL || sid < 0 ) {
				continue;
			}
			CBlob *blob = _newblobresult->GetBlob(i);
			CvRect blobrect = blob->GetBoundingBox();

			CvRect regionrect = r->_rect;
			CvPoint blobcenter = blob_center[i];

			float x = (float)blobcenter.x;
			float y = (float)blobcenter.y;

			float efactor = (float)(mmtt_values["expandfactor"]->value);
			float e_xmin = (float)(mmtt_values["expand_xmin"]->value);
			float e_xmax = (float)(mmtt_values["expand_xmax"]->value);
			float e_ymin = (float)(mmtt_values["expand_ymin"]->value);
			float e_ymax = (float)(mmtt_values["expand_ymax"]->value);
			normalize_region_xy(x,y,regionrect,efactor,e_xmin,e_xmax,e_ymin,e_ymax);

			int tuio_sid = tuioSessionId(r,sid);

			MmttSession* sess = r->_sessions[sid];
			// float depth = (float)(val_depth_detect_top.value - sess->_depth_mm) / (float)val_depth_detect_top.value;
			float depth = (float)sess->_depth_normalized;
			float f = (float)(blobrect.width*blobrect.height) / (float)(regionrect.width*regionrect.height);

			// Take into account some factors that can be used to adjust for differences due
			// to different palettes (e.g. the depth value in smaller palettes is small, so depthfactor
			// can be used to expand it.
			float dfactor = (float)(mmtt_values["depthfactor"]->value);
			depth = depth * dfactor;

			msg.clear();
			msg.setAddress("/tuio/25Dblb");
			msg.addStringArg("set");
			msg.addIntArg(tuio_sid);
			msg.addFloatArg(x);      // x (position)
			msg.addFloatArg(y);      // y (position)
			msg.addFloatArg(depth);        // z (position)
			msg.addFloatArg(0.0);          // a (angle)
			msg.addFloatArg((float)blobrect.width);   // w (width)
			msg.addFloatArg((float)blobrect.height);  // h (height)
			msg.addFloatArg(f);			   // f (area)
			msg.addFloatArg(0.0);          // X (velocity)
			msg.addFloatArg(0.0);          // Y (velocity)
			msg.addFloatArg(0.0);          // Z (velocity)
			msg.addFloatArg(0.0);          // A (rotational velocity)
			msg.addFloatArg(0.0);          // m (motion acceleration)
			msg.addFloatArg(0.0);          // r (rotation acceleration)
			bundle.addMessage(msg);
		}

		msg.clear();
		msg.setAddress("/tuio/25Dblb");
		msg.addStringArg("fseq");
		msg.addIntArg(_tuio_fseq++);
		bundle.addMessage(msg);

		SendAllOscClients(bundle,_Tuio1_25_Clients);
		SendOscToAllWebSocketClients(bundle);
	}
}

void
MmttServer::doTuio1_2D( int nactive, int numblobs, std::vector<int> &blob_sid, std::vector<MmttRegion*> &blob_region, std::vector<CvPoint> &blob_center) {
	OscBundle bundle;
	OscMessage msg;

	long tm = timeGetTime();
	if ( nactive == 0 ) {
		if ( (tm - _tuio_last_sent) > 100 ) {
			msg.clear();
			msg.setAddress("/tuio/2Dcur");
			msg.addStringArg("alive");
			bundle.addMessage(msg);
			msg.clear();
			msg.setAddress("/tuio/2Dcur");
			msg.addStringArg("fseq");
			msg.addIntArg(_tuio_fseq++);
			bundle.addMessage(msg);
			SendAllOscClients(bundle,_Tuio1_2_Clients);
			SendOscToAllWebSocketClients(bundle);
			_tuio_last_sent = tm;
		}
	} else {
		// Put out the TUIO messages

		bundle.clear();

		// First the "alive" message that lists all the active sessions
		msg.clear();
		msg.setAddress("/tuio/2Dcur");
		msg.addStringArg("alive");
		for ( int i=0; i<numblobs; i++ ) {
			MmttRegion* r = blob_region[i];
			int sid = blob_sid[i];
			if ( r == NULL || sid < 0 ) {
				continue;
			}
			CBlob *blob = _newblobresult->GetBlob(i);
			int tuio_sid = tuioSessionId(r,sid);
			msg.addIntArg(tuio_sid);
		}
		bundle.addMessage(msg);

		for ( int i=0; i<numblobs; i++ ) {
			MmttRegion* r = blob_region[i];
			int sid = blob_sid[i];
			if ( r == NULL || sid < 0 ) {
				continue;
			}
			CBlob *blob = _newblobresult->GetBlob(i);
			CvRect blobrect = blob->GetBoundingBox();

			CvRect regionrect = r->_rect;
			CvPoint blobcenter = blob_center[i];

			float x = (float)blobcenter.x;
			float y = (float)blobcenter.y;
			normalize_region_xy(x,y,regionrect);

			int tuio_sid = tuioSessionId(r,sid);

			// MmttSession* sess = r->_sessions[sid];
			// float f = (float)(blobrect.width*blobrect.height) / (float)(regionrect.width*regionrect.height);

			msg.clear();
			msg.setAddress("/tuio/2Dcur");
			msg.addStringArg("set");
			msg.addIntArg(tuio_sid);
			msg.addFloatArg(x);      // x (position)
			msg.addFloatArg(y);      // y (position)
			msg.addFloatArg(0.0);          // X (velocity)
			msg.addFloatArg(0.0);          // Y (velocity)
			msg.addFloatArg(0.0);          // m (motion acceleration)
			bundle.addMessage(msg);
		}

		msg.clear();
		msg.setAddress("/tuio/2Dcur");
		msg.addStringArg("fseq");
		msg.addIntArg(_tuio_fseq++);
		bundle.addMessage(msg);

		SendAllOscClients(bundle,_Tuio1_2_Clients);
		SendOscToAllWebSocketClients(bundle);
	}
}

void
MmttServer::copyRegionsToColorImage(IplImage* regions, unsigned char* pixels, bool overwriteBackground, bool reverseColor, bool reverseX)
{
	CvScalar c = cvScalar(0);
	for (int x=0; x<_camWidth; x++ ) {
		for (int y=0; y<_camHeight; y++ ) {
			int i = x + y*_camWidth;
			unsigned char g = regions->imageData[i];
			if ( ! overwriteBackground && g == 0 ) {
				continue;
			}
			// bit of a hack - sometimes the mask region id is MASK_REGION_ID, sometimes it's 255
			if ( g == 255 ) {
				g = MASK_REGION_ID;
			} else if ( g > MAX_REGION_ID ) {
				DEBUGPRINT(("Hey! invalid region value (%d)\n",g));
				g = MASK_REGION_ID;
			}
			c = region2cvscalar[g];
			if ( reverseX ) {
				i = (_camWidth-1-x) + y*_camWidth;
			}
			if ( reverseColor ) {
				pixels[i*3 + 0] = (unsigned char)c.val[2];
				pixels[i*3 + 1] = (unsigned char)c.val[1];
				pixels[i*3 + 2] = (unsigned char)c.val[0];
			} else {
				pixels[i*3 + 0] = (unsigned char)c.val[0];
				pixels[i*3 + 1] = (unsigned char)c.val[1];
				pixels[i*3 + 2] = (unsigned char)c.val[2];
			}
		}
	}
}

void
MmttServer::clearImage(IplImage* image)
{
	for (int x=0; x<_camWidth; x++ ) {
		for (int y=0; y<_camHeight; y++ ) {
			int i = x + y*_camWidth;
			image->imageData[i] = 0;   // black
		}
	}
}

void
MmttServer::copyColorImageToRegionsAndMask(unsigned char *pixels, IplImage* regions, IplImage* mask, bool reverseColor, bool reverseX)
{
	_regionsfilled = true;
	CvScalar c = cvScalar(0);
	for (int x=0; x<_camWidth; x++ ) {
		for (int y=0; y<_camHeight; y++ ) {
			int i = x + y*_camWidth;

			unsigned char r;
			unsigned char g;
			unsigned char b;

			if ( reverseColor ) {
				r = pixels[i*3 + 0];
				g = pixels[i*3 + 1];
				b = pixels[i*3 + 2];
			} else {
				r = pixels[i*3 + 2];
				g = pixels[i*3 + 1];
				b = pixels[i*3 + 0];
			}

			int region_id = regionOfColor(r,g,b);

			if ( reverseX ) {
				i = (_camWidth-1-x) + y*_camWidth;
			}
			regions->imageData[i] = region_id;

			if ( region_id == MASK_REGION_ID ) {
				mask->imageData[i] = (char)255;   // white
			} else {
				mask->imageData[i] = 0;   // black
			}
		}
	}
}

void
MmttServer::copyRegionRectsToRegionsImage(IplImage* regions, bool reverseColor, bool reverseX)
{
	_regionsfilled = true;

	int thick = CV_FILLED;

	for (int x=0; x<_camWidth; x++ ) {
		for (int y=0; y<_camHeight; y++ ) {
				int i = x + y*_camWidth;
				regions->imageData[i] = 0;
		}
	}

	size_t nregions = _curr_regions.size();
	for (size_t region_id=2; region_id<nregions; region_id++ ) {
		MmttRegion* r = _curr_regions[region_id];

		CvScalar c = region2cvscalar[region_id];

		CvRect rect = r->_rect;

		int x0 = rect.x;
		int x1 = rect.x + rect.width;
		int y0 = rect.y;
		int y1 = rect.y + rect.height;
		for (int x=x0; x<x1; x++ ) {
			for (int y=y0; y<y1; y++ ) {
				int i = x + y*_camWidth;
#if 0
				if ( reverseX ) {
					i = (_camWidth-1-x) + y*_camWidth;
				}
#endif
				regions->imageData[i] = (unsigned char) region_id;
			}
		}
	}
}

int
MmttServer::regionOfColor(int r, int g, int b)
{
	for ( int n=0; n<sizeof(region2color); n++ ) {
		RGBcolor color = region2color[n];
		// NOTE: swapping the R and B values, since OpenCV defaults to BGR
		if ( (r == color.b) && (g == color.g) && (b == color.r) ) {
			return n;
		}
	}
	return 0;
}



CvScalar
MmttServer::colorOfSession(int g)
{
	CvScalar c;
	switch (g) {
	case 0: c = CV_RGB(255,0,0); break;
	case 1: c = CV_RGB(0,255,0); break;
	case 2: c = CV_RGB(0,0,255); break;
	case 3: c = CV_RGB(255,255,0); break;
	case 4: c = CV_RGB(0,255,255); break;
	case 5: c = CV_RGB(255,0,255); break;
	default: c = CV_RGB(128,128,128); break;
	}
	return c;
}

MmttServer*
MmttServer::makeMmttServer(std::string configpath)
{
	DEBUGPRINT(("Calling Pt_Start in MmttServer"));
    Pt_Start(1, 0, 0);

	if ( NosuchNetworkInit() ) {
		DEBUGPRINT(("Unable to initialize networking?"));
		return NULL;
	}

	MmttServer* server = new MmttServer(configpath);

	std::string stat = server->status();
	if ( stat != "" ) {
		DEBUGPRINT(("Failure in creating MmttServer? status=%s",stat.c_str()));
		return NULL;
	}

	server->StartStuff();

	return server;
}
