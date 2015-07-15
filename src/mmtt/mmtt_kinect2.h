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
// #define DO_COLOR_FRAME
#define NO_DAEMON

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
struct IKinectSensor;
struct IDepthFrameReader;

#ifdef KINECT2_MULTIFRAMEREADER
struct IMultiSourceFrameReader;
#endif

#define _USE_MATH_DEFINES // To get definition of M_PI
#include <math.h>

// typedef interface IMultiSourceFrameReader IMultiSourceFrameReader;

class Kinect2DepthCamera : public DepthCamera {
public:
	Kinect2DepthCamera(MmttServer* s);
	~Kinect2DepthCamera();
	const int width() { return 512; };
	const int height() { return 424; };
	const int default_depth_detect_top() { return 1465; };
	const int default_depth_detect_bottom() { return 1420; };
	bool InitializeCamera();
	bool Update();
	void Shutdown();
	std::string camtype() { return "Kinect2"; }

private:

	void ProcessDepth(INT64 nTime, const UINT16* pBuffer, int nWidth, int nHeight, USHORT nMinDepth, USHORT nMaxDepth);
	void processRawDepth(const UINT16* depth, int width , int height );

    IKinectSensor*          m_pKinectSensor;
#ifndef KINECT2_MULTIFRAMEREADER
    IDepthFrameReader*      m_pDepthFrameReader;
#endif
    RGBQUAD*                m_pDepthRGBX;
#ifdef KINECT2_MULTIFRAMEREADER
    IMultiSourceFrameReader* m_pMultiSourceFrameReader;
#endif

#define MAX_DEPTH_CAMERAS 4
	int m_nsensors;
	int m_currentSensor;
	// INuiSensor* m_pNuiSensor[MAX_DEPTH_CAMERAS];
    HANDLE      m_hNextDepthFrameEvent[MAX_DEPTH_CAMERAS];
    HANDLE		m_pDepthStreamHandle[MAX_DEPTH_CAMERAS];
    HWND        m_hWnd;
    static const int        cStatusMessageMaxLen = MAX_PATH*2;

	static const int eventCount = 1;
	HANDLE hEvents[eventCount];
};


#endif
