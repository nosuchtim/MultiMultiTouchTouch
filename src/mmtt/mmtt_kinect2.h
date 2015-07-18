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

#ifndef MMTT_KINECT2_H
#define MMTT_KINECT2_H

#define KINECT2_MULTIFRAMEREADER
#define DO_MULTISOURCE
// #define DO_COLOR_FRAME
// #define USE_COLOR_FRAME
// #define DO_MAP_COLOR_FRAME

#include <vector>
#include <map>
#include <iostream>

#include "Kinect.h"

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
struct IKinectSensor;
struct IDepthFrameReader;

#ifdef DO_MULTISOURCE
struct IMultiSourceFrameReader;
#else
struct IDepthFrameReader;
#endif
struct ICoordinateMapper;

#define _USE_MATH_DEFINES // To get definition of M_PI
#include <math.h>

class Kinect2DepthCamera : public DepthCamera {
public:
	Kinect2DepthCamera(MmttServer* s);
	~Kinect2DepthCamera();
	const int width() { return 512; };
	const int height() { return 424; };
	const int default_depth_detect_top() { return 1465; };
	const int default_depth_detect_bottom() { return 1420; };
	bool InitializeCamera();
	bool Update(uint16_t *depthmm, uint8_t *depthbytes, uint8_t *threshbytes);
	void Shutdown();
	std::string camtype() { return "Kinect2"; }
	IplImage* colorimage();

private:

	void _processFrame(const UINT16* depth, int width , int height, uint16_t *depthmm, uint8_t *depthbytes, uint8_t *threshbytes
#ifdef DO_COLOR_FRAME
		, RGBQUAD * colorbytes, int colorwidth , int colorheight
#endif
		);

    IKinectSensor*          m_pKinectSensor;
    ICoordinateMapper*      m_pCoordinateMapper;
#ifdef DO_MULTISOURCE
    IMultiSourceFrameReader* m_pMultiSourceFrameReader;
#else
    IDepthFrameReader* m_pDepthFrameReader;
#endif

#ifdef DO_COLOR_FRAME
	IplImage*				m_colorImage;
	RGBQUAD*				m_colorbytes;
	int						m_colorWidth;
	int						m_colorHeight;
	ColorSpacePoint*		m_colorCoordinates;
	DepthSpacePoint*		m_depthCoordinates;
#endif

#define MAX_DEPTH_CAMERAS 4
	int m_nsensors;
	int m_currentSensor;
    HANDLE      m_hNextDepthFrameEvent[MAX_DEPTH_CAMERAS];
    HANDLE		m_pDepthStreamHandle[MAX_DEPTH_CAMERAS];
    HWND        m_hWnd;
};


#endif
