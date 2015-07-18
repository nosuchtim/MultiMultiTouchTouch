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

#include "mmtt_camera.h"

#ifdef KINECT2_CAMERA

#include "stdafx.h"
#include "Kinect.h"
#include "NosuchDebug.h"
#include "mmtt.h"

using namespace std;

Kinect2DepthCamera::Kinect2DepthCamera(MmttServer* s) {

	_server = s;

    m_pKinectSensor = NULL;
	m_pCoordinateMapper = NULL;
#ifdef DO_MULTISOURCE
    m_pMultiSourceFrameReader = NULL;
#else
    m_pDepthFrameReader = NULL;
#endif

#ifdef DO_COLOR_FRAME
	m_colorImage = NULL;
	m_colorbytes = NULL;
	m_colorCoordinates = NULL;
	m_depthCoordinates = NULL;
#endif
}

Kinect2DepthCamera::~Kinect2DepthCamera() {
	SafeRelease(m_pCoordinateMapper);
#ifdef DO_MULTISOURCE
	SafeRelease(m_pMultiSourceFrameReader);
#else
	SafeRelease(m_pDepthFrameReader);
#endif
	SafeRelease(m_pKinectSensor);

#ifdef DO_COLOR_FRAME
	cvReleaseImageHeader(&m_colorImage);
#endif
}

bool Kinect2DepthCamera::InitializeCamera()
{
    HRESULT hr;

    hr = GetDefaultKinectSensor(&m_pKinectSensor);
    if (FAILED(hr))
    {
        return false;
    }

	if (m_pKinectSensor)
	{
#ifndef DO_MULTISOURCE
		IDepthFrameSource* pDepthFrameSource = NULL;
#endif
		// Initialize the Kinect and get the depth reader
		hr = m_pKinectSensor->Open();

#ifdef DO_MULTISOURCE
		if (SUCCEEDED(hr))
		{
			hr = m_pKinectSensor->get_CoordinateMapper(&m_pCoordinateMapper);
		}

		if (SUCCEEDED(hr))
		{
			hr = m_pKinectSensor->OpenMultiSourceFrameReader(
#ifdef DO_COLOR_FRAME
				FrameSourceTypes::FrameSourceTypes_Depth | FrameSourceTypes::FrameSourceTypes_Color, &m_pMultiSourceFrameReader);
#else
				FrameSourceTypes::FrameSourceTypes_Depth, &m_pMultiSourceFrameReader);
#endif
			DEBUGPRINT(("opened multisource hr=%d", hr));
	}
#endif

#ifndef DO_MULTISOURCE
		if (SUCCEEDED(hr))
		{
			hr = m_pKinectSensor->get_DepthFrameSource(&pDepthFrameSource);
		}

		if (SUCCEEDED(hr))
		{
			hr = pDepthFrameSource->OpenReader(&m_pDepthFrameReader);
		}

		SafeRelease(pDepthFrameSource);
		DEBUGPRINT(("opened depthsource hr=%d", hr));
#endif
    }

    if (!m_pKinectSensor || FAILED(hr))
    {
        DEBUGPRINT(("No ready Kinect2 found!"));
        return false;
    }

    return true;
}

bool Kinect2DepthCamera::Update(uint16_t *depthmm, uint8_t *depthbytes, uint8_t *threshbytes)
{
	bool returnok = false;

#ifdef DO_MULTISOURCE
	if (!m_pMultiSourceFrameReader)
	{
		return(returnok);
	}
#endif

#ifdef DO_COLOR_FRAME
    IColorFrame* pColorFrame = NULL;
#endif

#ifndef DO_MULTISOURCE

	IDepthFrame* pDepthFrame = NULL;
	HRESULT hr = m_pDepthFrameReader->AcquireLatestFrame(&pDepthFrame);

#else
	IMultiSourceFrame* pMultiSourceFrame = NULL;
	HRESULT hr = m_pMultiSourceFrameReader->AcquireLatestFrame(&pMultiSourceFrame);

	if (SUCCEEDED(hr))
	{
		// DEBUGPRINT(("AcquireLatest OKAY!"));
		IDepthFrameReference* pDepthFrameReference = NULL;

		hr = pMultiSourceFrame->get_DepthFrameReference(&pDepthFrameReference);
		if (SUCCEEDED(hr))
		{
			hr = pDepthFrameReference->AcquireFrame(&pDepthFrame);
		}

		SafeRelease(pDepthFrameReference);
	}
	else {
		// DEBUGPRINT(("AcquireLatest NOT OKAY!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"));
		return(returnok);
	}
#endif

#ifdef DO_COLOR_FRAME
	if (SUCCEEDED(hr))
	{
		IColorFrameReference* pColorFrameReference = NULL;

		hr = pMultiSourceFrame->get_ColorFrameReference(&pColorFrameReference);
		if (SUCCEEDED(hr))
		{
			hr = pColorFrameReference->AcquireFrame(&pColorFrame);
		}

		SafeRelease(pColorFrameReference);
	}
#endif

#ifdef DO_COLOR_FRAME
	if (SUCCEEDED(hr) && pColorFrame && pDepthFrame)
#else
	if (SUCCEEDED(hr) && pDepthFrame)
#endif
	{
		INT64 nTime = 0;
		IFrameDescription* pFrameDepthDescription = NULL;
		int depthWidth = 0;
		int depthHeight = 0;
		USHORT nDepthMinReliableDistance = 0;
		USHORT nDepthMaxReliableDistance = 0;
		UINT nDepthBufferSize = 0;
		UINT16 *pDepthBuffer = NULL;
#ifdef DO_COLOR_FRAME
		UINT nColorBufferSize = 0;
		IFrameDescription* pFrameColorDescription = NULL;
		ColorImageFormat colorImageFormat = ColorImageFormat_None;
#endif

		hr = pDepthFrame->get_RelativeTime(&nTime);

		if (SUCCEEDED(hr))
		{
			hr = pDepthFrame->get_FrameDescription(&pFrameDepthDescription);
		}

		if (SUCCEEDED(hr))
		{
			hr = pFrameDepthDescription->get_Width(&depthWidth);
		}

		if (SUCCEEDED(hr))
		{
			hr = pFrameDepthDescription->get_Height(&depthHeight);
		}

		if (SUCCEEDED(hr))
		{
			hr = pDepthFrame->get_DepthMinReliableDistance(&nDepthMinReliableDistance);
		}

		if (SUCCEEDED(hr))
		{
			hr = pDepthFrame->get_DepthMaxReliableDistance(&nDepthMaxReliableDistance);
		}

		if (SUCCEEDED(hr))
		{
			hr = pDepthFrame->AccessUnderlyingBuffer(&nDepthBufferSize, &pDepthBuffer);
		}

#ifdef DO_COLOR_FRAME
		if (SUCCEEDED(hr))
		{
			hr = pColorFrame->get_FrameDescription(&pFrameColorDescription);
		}

		if (SUCCEEDED(hr))
		{
			hr = pFrameColorDescription->get_Width(&m_colorWidth);
		}

		if (SUCCEEDED(hr))
		{
			hr = pFrameColorDescription->get_Height(&m_colorHeight);
		}

		if (SUCCEEDED(hr))
		{
			hr = pColorFrame->get_RawColorImageFormat(&colorImageFormat);
		}

		UINT colorbuffersize = m_colorWidth * m_colorHeight * sizeof(RGBQUAD);

		if (m_colorbytes == NULL) {
			m_colorbytes = (RGBQUAD*)malloc(colorbuffersize);
		}

        RGBQUAD *pColorBuffer = NULL;

		if (SUCCEEDED(hr))
		{
			if (colorImageFormat == ColorImageFormat_Bgra)
			{
				UINT cbsize = 0;
				hr = pColorFrame->AccessRawUnderlyingBuffer(&cbsize, reinterpret_cast<BYTE**>(&pColorBuffer));
				memcpy(m_colorbytes, pColorBuffer, cbsize);
			}
			else if (m_colorbytes)
			{
				pColorBuffer = m_colorbytes;
				hr = pColorFrame->CopyConvertedFrameDataToArray(colorbuffersize, reinterpret_cast<BYTE*>(pColorBuffer), ColorImageFormat_Bgra);
			}
			else
			{
				hr = E_FAIL;
			}
		}

		int depthSize = depthWidth * depthHeight;
		int colorSize = m_colorWidth * m_colorHeight;

		if (m_colorImage == NULL) {
			m_colorImage = cvCreateImageHeader(cvSize(m_colorWidth, m_colorHeight), IPL_DEPTH_8U, 4);
		}
		if (m_colorCoordinates == NULL) {
			m_colorCoordinates = new ColorSpacePoint[depthSize];
		}
		if (m_depthCoordinates == NULL) {
			m_depthCoordinates = new DepthSpacePoint[colorSize];
		}
#endif

        if (SUCCEEDED(hr))
        {
			// Make sure we've received valid data
#ifdef DO_COLOR_FRAME
			if (pDepthBuffer && pColorBuffer && (depthWidth == width()) && (depthHeight == height())) {
#else
			if (pDepthBuffer && (depthWidth == width()) && (depthHeight == height())) {
#endif

				_processFrame(
					pDepthBuffer, width(), height(), depthmm, depthbytes, threshbytes
#ifdef DO_COLOR_FRAME
					,pColorBuffer, m_colorWidth, m_colorHeight
#endif
					);
			}
		}

        SafeRelease(pFrameDepthDescription);
#ifdef DO_COLOR_FRAME
        SafeRelease(pFrameColorDescription);
#endif
		returnok = true;
    }

    SafeRelease(pDepthFrame);

#ifdef DO_MULTISOURCE
	SafeRelease(pMultiSourceFrame);
#endif

#ifdef DO_COLOR_FRAME
    SafeRelease(pColorFrame);
#endif
	return(returnok);
}

void
Kinect2DepthCamera::_processFrame(const UINT16* pDepthBuffer, int depthWidth, int depthHeight,
				uint16_t *depthmm, uint8_t *depthbytes, uint8_t *threshbytes
#ifdef DO_COLOR_FRAME
				, RGBQUAD* pColorBuffer, int colorWidth, int colorHeight
#endif
				)
{

	int i = 0;
	bool showgreyscale = (server()->val_showgreyscaledepth.value != 0);
	bool filterdepth = !server()->val_showrawdepth.value;

	// XXX - THIS NEEDS OPTIMIZATION!

	const UINT16* pdbuffer = pDepthBuffer;
	for (int y = 0; y < depthHeight; y++) {
		float backval = (float)(server()->val_depth_detect_top.value
			+ (server()->val_depth_detect_bottom.value - server()->val_depth_detect_top.value)
			* (float(y) / depthHeight));

		for (int x = 0; x < depthWidth; x++, i++) {
			USHORT d = *pdbuffer++;

			// This is a bottleneck
			uint16_t mm = d;

			depthmm[i] = mm;
#define OUT_OF_BOUNDS 99999
			int deltamm;
			int pval = 0;
			if (filterdepth) {
				if (mm == 0 || mm < server()->val_depth_front.value || mm > backval) {
					pval = OUT_OF_BOUNDS;
				}
				else if (x < server()->val_edge_left.value || x > server()->val_edge_right.value || y < server()->val_edge_top.value || y > server()->val_edge_bottom.value) {
					pval = OUT_OF_BOUNDS;
				}
				else {
					// deltamm = (int)server()->val_depth_detect_top.value - mm;
					deltamm = (int)backval - mm;
				}
			}
			else {
				deltamm = (int)backval - mm;
			}
			if (pval == OUT_OF_BOUNDS || (pval >> 8) > 5) {
				// black
				*depthbytes++ = 0;
				*depthbytes++ = 0;
				*depthbytes++ = 0;
				threshbytes[i] = 0;
			}
			else {
				// white
				uint8_t mid = 255;
				if (showgreyscale) {
					// a little grey-level based on distance
					mid = mid - (deltamm / 10);
				}
				*depthbytes++ = mid;
				*depthbytes++ = mid;
				*depthbytes++ = mid;
				threshbytes[i] = 255;
			}
		}
	}

#ifdef DO_COLOR_FRAME
	int depthSize = depthWidth * depthHeight;
	int colorSize = colorWidth * colorHeight;

#ifdef DO_MAP_COLOR_FRAME
    HRESULT hr;

	hr = m_pCoordinateMapper->MapColorFrameToDepthSpace(depthSize, pDepthBuffer, colorSize, m_depthCoordinates);
	if (FAILED(hr))
	{
		DEBUGPRINT(("MapColorFrameToDepthSpace failed!?"));
		return;
	}

#if 0
	for (int x = 0; x < colorWidth; x++) {
		for (int y = 0; y < colorHeight; y++) {
			DepthSpacePoint pt = m_depthCoordinates[y*depthWidth + x];
			DEBUGPRINT(("depthcoord x=%d y=%d mapped to x=%.3lf y=%.3lf", x, y, pt.X, pt.Y));
		}
	}
#endif
	     
#if 0
	hr = m_pCoordinateMapper->MapDepthFrameToColorSpace(depthSize, pDepthBuffer, colorSize, m_colorCoordinates);

	if (FAILED(hr))
	{
		DEBUGPRINT(("MapDepthFrameToColorSpace failed!?"));
		return;
	}
	else {
		for (int depthIndex = 0; depthIndex < depthSize; depthIndex++ ) {
			ColorSpacePoint csp = m_colorCoordinates[depthIndex];
		}
	}
#endif
#endif

	m_colorImage->origin = 1;
	m_colorImage->imageData = (char*)m_colorbytes;
#endif
}

IplImage*
Kinect2DepthCamera::colorimage() {
#ifdef DO_COLOR_FRAME
	return m_colorImage;
#else
	return NULL;
#endif
}


void Kinect2DepthCamera::Shutdown() {
}

#endif
