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

#ifndef MMTT_H
#define MMTT_H

#include "mmtt_camera.h"

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

#include "spout.h"

class Cursor;

class UT_SharedMem;
class MMTT_SharedMemHeader;
class TOP_SharedMemHeader;

#define _USE_MATH_DEFINES // To get definition of M_PI
#include <math.h>

struct Globals
{
    HINSTANCE hInstance;    // window app instance
    HWND hwnd;      // handle for the window
    HDC   hdc;      // handle to device context
    HGLRC hglrc;    // handle to OpenGL rendering context
    int width, height;      // the desired width and
    // height of the CLIENT AREA
    // (DRAWABLE REGION in Window)
};

///////////////////////////
// GLOBALS
// declare one struct Globals called g;
// extern Globals g;

class MmttHttpServer;
class MmttServer;
class NosuchSocketConnection;

// hack to get function pointers, can be fixed by proper casting
extern MmttServer* ThisServer;

class MmttValue {
public:
	MmttValue() {
		MmttValue(0.0,0.0,0.0);
	}
	MmttValue(double vl,double mn,double mx,bool persist = true) {
		minvalue = mn;
		maxvalue = mx;
		value = vl;
		persistent = persist;
	}
	void set_persist(bool p) { persistent = p; }
	void set_value(double v) {
		if ( v < minvalue )
			v = minvalue;
		else if ( v > maxvalue )
			v = maxvalue;
		value = v;
	}
	double value;
	double minvalue;
	double maxvalue;
	bool persistent;
};

class MmttSession {
public:
	MmttSession(CBlob* b, CvPoint center, int framenum) {
		_blob = b;
		_center = center;
		_depth_mm = 0;
		_depth_normalized = 0.0;
		_frame_born = framenum;
	}
	CBlob* _blob;
	CvPoint _center;
	int _depth_mm;
	float _depth_normalized;
	int _frame_born;
};

class MmttRegion {
public:
	MmttRegion(int i, int first_sid, CvRect r) {
		id = i;
		_first_sid = first_sid;
		_rect = r;
		name = NosuchSnprintf("Region%d",id);
		DEBUGPRINT1(("==== NEW MmttRegion id=%d _first_sid=%d",_id,_first_sid));
	}
	~MmttRegion() {
		DEBUGPRINT(("MmttRegion DESTRUCTOR called! id=%d _first_id=%d\n",id,_first_sid));
	}
	int id;
	std::string name;
	int _first_sid;
	CvRect _rect;
	// std::vector<CBlob*> _blobs;
	// std::vector<int> _blobs_sid;  // session ids assigned to blobs
	std::map<int,MmttSession*> _sessions;    // map from session id to MmttSession

	int getAvailableSid();
};

class MmttServer : public NosuchJsonListener {
 public:
	MmttServer(std::string configpath);
	~MmttServer();

	static MmttServer* makeMmttServer(std::string configpath);
	static void ErrorPopup(LPCWSTR msg);
	static void ErrorPopup(const char* msg);

	void InitOscClientLists();
	void SetOscClientList(std::string& clientlist,std::vector<OscSender*>& clientvector);
	void SendOscToClients();
	UT_SharedMem* setup_shmem_for_image();
	UT_SharedMem* setup_shmem_for_outlines();
	void shmem_lock_and_update_image(unsigned char* pix);
	void shmem_update_image(TOP_SharedMemHeader* h, unsigned char* pix);
	void shmem_lock_and_update_outlines(int nactive, int numblobs, std::vector<int> &blob_sid, std::vector<MmttRegion*> &blob_region, std::vector<CvPoint> &blob_center);
	void shmem_update_outlines(MMTT_SharedMemHeader* h,
		int nactive, int numblobs, std::vector<int> &blob_sid, std::vector<MmttRegion*> &blob_region, std::vector<CvPoint> &blob_center);
	void shutdown();
	void check_json_and_execute();
	void flip_images();
	void analyze_depth_images();
	void draw_depth_image(unsigned char* pix);
	void draw_color_image();
	void draw_begin();
	void draw_end();
	void swap_buffers();
	int get_flip_mode();

	LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam ); 

	Globals g;

	void Update();
    int Run(std::string title, HINSTANCE hInstance, int nCmdShow);
    LRESULT CALLBACK        DlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    void   SetStatusMessage(WCHAR* szMessage);

	void ReSizeGLScene();	// Resize And Initialize The GL Window

	void *mmtt_json_threadfunc(void *arg);

	CvScalar colorOfSession(int g);
	int regionOfColor(int r, int g, int b);

	std::string SavePatch(std::string prefix);
	void deriveRegionsFromImage();
	void startNewRegions();
	void finishNewRegions();
	std::string LoadPatch(std::string prefix, const char* id = NULL);
	std::string LoadPatchReal(std::string prefix);
	std::string LoadPatchJson(std::string jstr);
	std::string startAlign(bool reload);
	void StartStuff();

	bool shutting_down;
	std::string status() { return _status; }

	virtual std::string submitJson(std::string meth, cJSON *params, const char *id);

	std::string processJson(std::string method, cJSON *params, const char *id);
	std::string AdjustValue(cJSON *params, const char *id, int direction);
	void SendOscToAllWebSocketClients(OscBundle& bundle);

	void SendAllOscClients(OscBundle& bundle);
	void SendAllOscClients(OscBundle& bundle, std::vector<OscSender *> &oscClients);

	unsigned char *ffpixels() { return _ffpixels; }

	UT_SharedMem *_sharedmem_image;
	UT_SharedMem *_sharedmem_outlines;

	DepthCamera* camera;

	MmttValue val_showfps;
	MmttValue val_flipx;
	MmttValue val_flipy;
	MmttValue val_showrawdepth;
	MmttValue val_showregionhits;
	MmttValue val_showregionmap;
	MmttValue val_showregionrects;
	MmttValue val_showgreyscaledepth;
	MmttValue val_showmask;
	MmttValue val_usemask;
	MmttValue val_tilt;
	MmttValue val_edge_left;
	MmttValue val_edge_right;
	MmttValue val_edge_top;
	MmttValue val_edge_bottom;
	MmttValue val_depth_front; // mm
	MmttValue val_depth_detect_top;  // mm
	MmttValue val_depth_detect_bottom; // mm
	MmttValue val_depth_align;  // mm - depth used (only) when doing alignment
	MmttValue val_auto_window; // mm
	MmttValue val_blob_filter;
	MmttValue val_blob_param1;
	MmttValue val_blob_maxsize;
	MmttValue val_blob_minsize;
	MmttValue val_confidence;
	MmttValue val_depthfactor;
	MmttValue val_expandfactor;
	MmttValue val_expand_xmin;
	MmttValue val_expand_xmax;
	MmttValue val_expand_ymin;
	MmttValue val_expand_ymax;

	bool continuousAlign;

private:

	SpoutSender _spoutDepthSender;
#ifdef DO_COLOR_FRAME
	SpoutSender _spoutColorSender;
#endif

	void init_regular_values();
	void init_camera_values();
	std::string LoadGlobalDefaults();
	void LoadConfigDefaultsJson(cJSON* json);
	void doRegistration();
	void doDepthRegistration();
	void doAutoDepthRegistration();
	void doManualDepthRegistration();
	void doPokeRegistration();
	void registrationStart();
	void registrationContinueManual();
	void registrationPoke(CvPoint pt);
	void analyzePixels();
	void showMask();
	void showRegionHits();
	void showRegionRects();
	void showBlobSessions();
	void clearImage(IplImage* image);
	void removeMaskFrom(uint8_t* pixels);
	void copyRegionsToColorImage(IplImage* regions, unsigned char* pixels, bool overwriteBackground, bool reverseColor, bool reverseX );
	void copyColorImageToRegionsAndMask(unsigned char* pixels, IplImage* regions, IplImage* mask, bool reverseColor, bool reverseX );
	void copyRegionRectsToRegionsImage(IplImage* regions, bool reverseColor, bool reverseX);
	void doTuio1_25D( int nactive, int numblobs, std::vector<int> &blob_sid, std::vector<MmttRegion*> &blob_region, std::vector<CvPoint> &blob_center);
	void doTuio1_2D( int nactive, int numblobs, std::vector<int> &blob_sid, std::vector<MmttRegion*> &blob_region, std::vector<CvPoint> &blob_center);
	void doTuio2( int nactive, int numblobs, std::vector<int> &blob_sid, std::vector<MmttRegion*> &blob_region, std::vector<CvPoint> &blob_center);

	void _updateValue(std::string nm, MmttValue* v);
	void _toggleValue(MmttValue* v);
	void _stop_registration();

	void camera_setup(std::string camname = "");

	bool			mFirstDraw;

	bool first_client_option;
	std::string _Tuio1_2_OscClientList;
	std::string _Tuio1_25_OscClientList;
	std::string _Tuio2_OscClientList;
	std::vector<OscSender *> _Tuio1_2_Clients;
	std::vector<OscSender *> _Tuio1_25_Clients;
	std::vector<OscSender *> _Tuio2_Clients;

	std::string _configpath;
	bool _do_sharedmem;
	std::string _sharedmemname;
	bool _do_tuio;
	bool _do_errorpopup;
	bool _do_initialalign;
	bool _python_disabled;
	bool _do_python;
	std::string _status;

	int _jsonport;
	std::string _htmldir;
	std::string _patchFile;
	std::string _cameraType;
	int _cameraIndex; // when there are multiple cameras
	std::string _tempDir;
	int	_camWidth;
	int	_camHeight;
	int _camBytesPerPixel;
	int _fpscount;
	int _framenum;
	bool _regionsfilled;
	bool _regionsDefinedByPatch;
	CvSize _camSize;
	int _camSizeInBytes;
	IplImage* _ffImage;
	IplImage* _tmpGray;
	IplImage* _maskImage;
	IplImage* _regionsImage;
	IplImage* _tmpRegionsColor;
	IplImage* _tmpThresh;

	unsigned char *_ffpixels;
	size_t _ffpixelsz;

	NosuchDaemon*	_httpserver;

	int _tuio_fseq;
	int _tuio_last_sent;

	bool do_setnextregion;
	std::map<std::string, MmttValue*> mmtt_values;

	CBlobResult* _newblobresult;
	CBlobResult* _oldblobresult;
	std::vector<MmttRegion*> _curr_regions;
	std::vector<MmttRegion*> _new_regions;
	std::vector<CvPoint> _savedpokes;

	double lastFpsTime;
	bool doBlob;
	bool doBW;
	bool doSmooth;
	bool autoDepth;
	// CvPoint crosshairPoint;
	int currentRegionValue;
	int registrationState;  // if 0, registration is not currently underway
	bool continuousAlignStop;
	int continuousAlignOkayCount;
	int continuousAlignOkayEnough;
	
	uint8_t *depth_copy;
	uint8_t *depth_front;

	uint16_t *depthmm_mid;
	uint8_t *thresh_mid;
	uint8_t *depth_mid;

	uint16_t *depthmm_front;
	uint8_t *thresh_front;

	pthread_mutex_t json_mutex;
	pthread_cond_t json_cond;

	bool json_pending;
	std::string json_method;
	cJSON* json_params;
	const char *json_id;
	std::string json_result;

};

std::string MmttForwardSlash(std::string s);
bool isFullPath(std::string p);
std::string MmttPath(std::string fname);
std::string MmttLogDir();

#endif
