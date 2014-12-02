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

#include "stdafx.h"
#include "Kinect.h"
#include "NosuchDebug.h"
#include "mmtt.h"
#include "mmtt_kinect2.h"

#ifdef KINECT2_CAMERA

using namespace std;

Kinect2DepthCamera::Kinect2DepthCamera(MmttServer* s) {

	_server = s;

    m_pKinectSensor = NULL;
    m_pDepthFrameReader = NULL;
    m_pDepthRGBX = NULL;

    // create heap storage for depth pixel data in RGBX format
    m_pDepthRGBX = new RGBQUAD[width() * height()];
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
        // Initialize the Kinect and get the depth reader
        IDepthFrameSource* pDepthFrameSource = NULL;

        hr = m_pKinectSensor->Open();

        if (SUCCEEDED(hr))
        {
            hr = m_pKinectSensor->get_DepthFrameSource(&pDepthFrameSource);
        }

        if (SUCCEEDED(hr))
        {
            hr = pDepthFrameSource->OpenReader(&m_pDepthFrameReader);
        }

        SafeRelease(pDepthFrameSource);
    }

    if (!m_pKinectSensor || FAILED(hr))
    {
        DEBUGPRINT(("No ready Kinect2 found!"));
        return false;
    }

    return true;
}

void Kinect2DepthCamera::Update()
{
    if (!m_pDepthFrameReader)
    {
        return;
    }

    IDepthFrame* pDepthFrame = NULL;

    HRESULT hr = m_pDepthFrameReader->AcquireLatestFrame(&pDepthFrame);

    if (SUCCEEDED(hr))
    {
        INT64 nTime = 0;
        IFrameDescription* pFrameDescription = NULL;
        int nWidth = 0;
        int nHeight = 0;
        USHORT nDepthMinReliableDistance = 0;
        USHORT nDepthMaxReliableDistance = 0;
        UINT nBufferSize = 0;
        UINT16 *pBuffer = NULL;

        hr = pDepthFrame->get_RelativeTime(&nTime);

        if (SUCCEEDED(hr))
        {
            hr = pDepthFrame->get_FrameDescription(&pFrameDescription);
        }

        if (SUCCEEDED(hr))
        {
            hr = pFrameDescription->get_Width(&nWidth);
        }

        if (SUCCEEDED(hr))
        {
            hr = pFrameDescription->get_Height(&nHeight);
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
            hr = pDepthFrame->AccessUnderlyingBuffer(&nBufferSize, &pBuffer);            
        }

        if (SUCCEEDED(hr))
        {
            ProcessDepth(nTime, pBuffer, nWidth, nHeight, nDepthMinReliableDistance, nDepthMaxReliableDistance);
        }

        SafeRelease(pFrameDescription);
    }

    SafeRelease(pDepthFrame);
}

/// <summary>
/// Handle new depth data
/// <param name="nTime">timestamp of frame</param>
/// <param name="pBuffer">pointer to frame data</param>
/// <param name="nWidth">width (in pixels) of input image data</param>
/// <param name="nHeight">height (in pixels) of input image data</param>
/// <param name="nMinDepth">minimum reliable depth</param>
/// <param name="nMaxDepth">maximum reliable depth</param>
/// </summary>

void Kinect2DepthCamera::ProcessDepth(INT64 nTime, const UINT16* pBuffer, int nWidth, int nHeight, USHORT nMinDepth, USHORT nMaxDepth)
{
    // Make sure we've received valid data
    if (m_pDepthRGBX && pBuffer && (nWidth == width()) && (nHeight == height())) {
        processRawDepth(pBuffer, width() , height());
    }
}

void
Kinect2DepthCamera::processRawDepth(const UINT16* pBuffer, int width , int height)
{
	uint16_t *depthmm = server()->depthmm_mid;
	uint8_t *depthbytes = server()->depth_mid;
	uint8_t *threshbytes = server()->thresh_mid;

	int i = 0;

	bool filterdepth = ! server()->val_showrawdepth.value;

	// XXX - THIS NEEDS OPTIMIZATION!

	// UINT16 *pdepth = depth;

	for (int y=0; y<height; y++) {
	  float backval = (float)(server()->val_depth_detect_top.value
		  + (server()->val_depth_detect_bottom.value - server()->val_depth_detect_top.value)
		  * (float(y)/height));

	  for (int x=0; x<width; x++,i++) {
		// int d = depth[i].depth;
		// USHORT d = (pdepth++)->depth;
		USHORT d = *pBuffer++;

		// This is a bottleneck
		uint16_t mm = d;

		depthmm[i] = mm;
#define OUT_OF_BOUNDS 99999
		int deltamm;
		int pval = 0;
		if ( filterdepth ) {
			if ( mm == 0 || mm < server()->val_depth_front.value || mm > backval ) {
				pval = OUT_OF_BOUNDS;
			} else if ( x < server()->val_edge_left.value || x > server()->val_edge_right.value || y < server()->val_edge_top.value || y > server()->val_edge_bottom.value ) {
				pval = OUT_OF_BOUNDS;
			} else {
				// deltamm = (int)server()->val_depth_detect_top.value - mm;
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

void Kinect2DepthCamera::Shutdown() {
}

#endif
