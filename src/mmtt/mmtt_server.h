/*
	Space Manifold - a variety of tools for Kinect and FreeFrame

	Copyright (c) 2011-2012 Tim Thompson <me@timthompson.com>

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

#ifndef MMTT_SERVER_H
#define MMTT_SERVER_H

#include <vector>
#include <map>
#include <iostream>

#include "ip/NetworkingUtils.h"

#include "OscSender.h"
#include "OscMessage.h"
#include "BlobResult.h"
#include "blob.h"

#include "NosuchHttp.h"
#include "NosuchException.h"

#define USE_LEAP

#ifdef USE_LEAP
#include "Leap.h"
#endif

#define _USE_MATH_DEFINES // To get definition of M_PI
#include <math.h>

#define FREENECT_FRAME_W 640
#define FREENECT_FRAME_H 480

class MmttHttp;
class MmttServer;
class NosuchSocketConnection;

class MmttValue {
public:
	MmttValue() {
		MmttValue(0.0,0.0,0.0);
	}
	MmttValue(double vl,double mn,double mx,bool persist = true) {
		minvalue = mn;
		maxvalue = mx;
		internal_value = vl;
		external_value = (internal_value-mn) / (mx-mn);
		persistent = persist;
	}
	void set_persist(bool p) { persistent = p; }
	void set_internal_value(double v) {
		if ( v < minvalue )
			v = minvalue;
		else if ( v > maxvalue )
			v = maxvalue;
		internal_value = v;
		external_value = (internal_value - minvalue) / (maxvalue-minvalue);
	}
	void set_external_value(double v) {
		if ( v < 0.0 )
			v = 0.0;
		else if ( v > 1.0 )
			v = 1.0;
		external_value = v;
		internal_value = minvalue + v * (maxvalue-minvalue);
	}
	double internal_value;
	double external_value; // always 0.0 to 1.0
	double minvalue;
	double maxvalue;
	bool persistent;
};

class LeapFinger {
};

class MmttSession {
public:
	MmttSession(int hfid = -1) {
		_blob = NULL;
		_center = cvPoint(0,0);
		_depth_mm = 0;
		_leap_hfid = hfid;
	}
	MmttSession(CBlob* b) {
		_blob = b;
		CvRect r = b->GetBoundingBox();
		_center = cvPoint(r.x + r.width/2, r.y + r.height/2);
		_depth_mm = 0;
		_leap_hfid = -1;
	}
	CBlob* _blob;
	CvPoint _center;
	int _depth_mm;
	float _depth_normalized;
	int _leap_hfid;
	// Leap::Vector _leap_pt;
};

class MmttRegion {
public:
	MmttRegion(int i, int first_sid, CvRect r) {
		id = i;
		_first_sid = first_sid;
		_rect = r;
		NosuchDebug(1,"==== NEW MmttRegion id=%d _first_sid=%d",id,_first_sid);
	}
	~MmttRegion() {
		NosuchDebug("MmttRegion DESTRUCTOR called! _id=%d\n",_first_sid);
	}
	int id;
	int _first_sid;
	CvRect _rect;
	// std::vector<CBlob*> _blobs;
	// std::vector<int> _blobs_sid;  // session ids assigned to blobs
	std::map<int,MmttSession> _sessions;    // map from session id to MmttSession

	int getAvailableSid();
};

#ifdef USE_LEAP
class LeapListener : public Leap::Listener {
  public:
	LeapListener(MmttServer* mmtt);
	virtual void onInit(const Leap::Controller&);
	virtual void onConnect(const Leap::Controller&);
	virtual void onDisconnect(const Leap::Controller&);
	virtual void onFrame(const Leap::Controller&);
private:
	MmttServer* _mmtt;
};
#endif

class MmttServer {
 public:
	MmttServer();
	~MmttServer();

	void InitOscClientLists();
	void SetOscClientList(std::string& clientlist,std::vector<OscSender*>& clientvector);
	void SendOscToClients();
	void setup();
	void ErrorPopup(LPCWSTR msg);
	void ErrorPopup(const char* msg);
	bool setup_capture_camera(std::string camname);
	void shutdown();
	void update();
	void draw();
	void set_autoupdate(bool t) { _autoupdate = t; }

	void update_kinect_images();
	void draw_kinect();
	bool setup_kinect_camera();
	void *real_kinect_freenect_threadfunc(void *arg);
	void real_kinect_depth_cb(freenect_device *dev, void *v_depth, uint32_t timestamp);
	void real_kinect_rgb_cb(freenect_device *dev, void *rgb, uint32_t timestamp);
	void processRawDepth(uint16_t *depth);
	bool is_kinectable() { return _kinectable; }

	void *real_mmtt_json_threadfunc(void *arg);

	CvScalar colorOfSession(int g);
	int regionOfColor(int r, int g, int b);

	std::string SavePatch(std::string prefix, const char* id);
	void deriveRegionsFromImage();
	void startNewRegions();
	void finishNewRegions();
	std::string LoadPatch(std::string prefix);
	std::string LoadPatchJson(std::string jstr);
	void LoadGlobalDefaults();
	void LoadConfigDefaultsJson(std::string jstr);

	bool shutting_down;

	std::string RespondToJson(const char *method, cJSON *params, const char *id);
	std::string ExecuteJson(const char *method, cJSON *params, const char *id);
	std::string AdjustValue(cJSON *params, const char *id, int direction);
	void SendOscToAllWebSocketClients(OscBundle& bundle);
	// std::string error_json(int code, const char *msg, const char *id);

	// void kinect_set(std::string name, double val);
	// double kinect_get(std::string name);

	void SendAllOscClients(OscBundle& bundle);
	void SendAllOscClients(OscBundle& bundle, std::vector<OscSender *> &oscClients);

	int	textureWidth() { return _textureWidth; }
	int	textureHeight() { return _textureHeight; }
	unsigned char *ffpixels() { return _ffpixels; }

	void gl_lock();
	void gl_unlock();

	void init_values();

#ifdef USE_LEAP
	bool setup_leap_camera();
	bool is_leapable() { return _leapable; }
	void draw_leap();

	void doTuio1_25D( const std::vector<Leap::Hand>& hands );
	void set_draw_leap(bool on);
	bool _draw_leap;
	LeapListener *_leap_listener;
	Leap::Controller *_leap_controller;
	bool _leapable;
#endif

private:

	// Kinect stuff
	void set_draw_kinect(bool on);
	bool _draw_kinect;

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
	bool _autoupdate;

	bool first_client_option;
	std::string _Tuio1_2_OscClientList;
	std::string _Tuio1_25_OscClientList;
	std::string _Tuio2_OscClientList;
	std::vector<OscSender *> _Tuio1_2_Clients;
	std::vector<OscSender *> _Tuio1_25_Clients;
	std::vector<OscSender *> _Tuio2_Clients;

	int _jsonport;
	std::string _patchFile;
	int	_winWidth;
	int	_winHeight;
	int	_textureWidth;
	int	_textureHeight;
	int	_camWidth;
	int	_camHeight;
	int _camFrames;
	int _anaFrames;
	int _depthFrames;
	bool _regionsfilled;
	bool _regionsDefinedByPatch;
	bool _showrawdepth;
	bool _showregionrects;
	CvSize _camSize;
	IplImage* _ffImage;
	IplImage* _tmpGray;
	IplImage* _maskImage;
	IplImage* _regionsImage;
	IplImage* _tmpRegionsColor;
	IplImage* _tmpThresh;
	// CvMemStorage* _tmpStorage;

	unsigned char *_ffpixels;
	size_t _ffpixelsz;

	MmttHttp*	_httpserver;

	int _tuio_fseq;
	int _tuio_last_sent;
	int _tuio_flip_x;
	int _tuio_flip_y;

	bool do_setnextregion;
	std::map<std::string, MmttValue*> mmtt_values;

	CBlobResult* _newblobresult;
	CBlobResult* _oldblobresult;
	std::vector<MmttRegion*> _curr_regions;
	std::vector<MmttRegion*> _new_regions;
	std::vector<CvPoint> _savedpokes;

	double lastFpsTime;
	int nFps;
	bool doBlob;
	bool doBW;
	bool doSmooth;
	bool autoDepth;
	// CvPoint crosshairPoint;
	int currentRegionValue;
	int registrationState;  // if 0, registration is not currently underway
	
	freenect_device *f_dev;
	int freenect_angle;
	freenect_video_format requested_format;
	freenect_video_format current_format;
	uint8_t *depth_copy;
	uint8_t *depth_mid, *depth_front;
	uint16_t *rawdepth_mid, *rawdepth_back, *rawdepth_front;
	uint16_t *depthmm_mid, *depthmm_front;
	uint8_t *thresh_mid, *thresh_front;
	uint16_t *depth_mm;
#ifdef DO_RGB_VIDEO
	uint8_t *rgb_back, *rgb_mid, *rgb_front;
#endif
	freenect_context *f_ctx;
	pthread_mutex_t gl_mutex;
	pthread_mutex_t gl_backbuf_mutex;
	pthread_cond_t gl_frame_cond;

	pthread_mutex_t json_mutex;
	pthread_cond_t json_cond;

	bool json_pending;
	const char *json_method;
	cJSON* json_params;
	const char *json_id;
	std::string json_result;

	int got_rgb;
	int got_depth;

#define T_GAMMA_SIZE 2048
	uint16_t t_gamma[T_GAMMA_SIZE];
	uint16_t raw_depth_to_mm[T_GAMMA_SIZE];

	bool _kinectable;
	
	MmttValue val_debug;
	MmttValue val_showfps;
	// MmttValue val_showkinect;
	MmttValue val_showrawdepth;
	MmttValue val_showregionhits;
	MmttValue val_showregionmap;
	MmttValue val_showregionrects;
	MmttValue val_showmask;
	MmttValue val_usemask;
	MmttValue val_tilt;
	MmttValue val_left;
	MmttValue val_right;
	MmttValue val_top;
	MmttValue val_bottom;
	MmttValue val_front; // mm
	MmttValue val_backtop;  // mm
	MmttValue val_backbottom; // mm
	MmttValue val_auto_window; // mm
	MmttValue val_blob_param1;
	MmttValue val_blob_maxsize;
	MmttValue val_blob_minsize;

};

class MmttHttp: public NosuchHttp {
public:
	MmttHttp(MmttServer *app, int port, int timeout) : NosuchHttp(port, timeout) {
		_server = app;
	}
	~MmttHttp() {
	}
	std::string RespondToJson(const char *method, cJSON *params, const char *id) {
		return _server->RespondToJson(method, params, id);
	}
private:
	MmttServer* _server;
};

extern MmttServer* ThisServer;

extern std::string GlobalDefaultsFile;
extern std::string MmttPatchDir;

bool mmtt_server_create(const char *homedir);
bool mmtt_server_isrunning();
void mmtt_server_update();
void mmtt_server_set_autoupdate(bool t);
void mmtt_server_draw();
std::string mmtt_server_execute(std::string method, std::string params);
void mmtt_server_destroy();

#endif
