#ifndef FFUTIL_H
#define FFUTIL_H

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

#include "FFGL.h"
#include "FFGLPlugin.h"
#include "FFGL.h"
#include "FFGLLib.h"
#include "FFPlugin.h"
#include "FreeFrame.h"

#include <FFGLFBO.h>

#define MAXPLUGINS 150

#define NPREPLUGINS 3
#define MAXFFGLRUNNING 10

class FFPluginDef;
class FFGLPluginDef;

extern int nffplugindefs;
extern FFPluginDef *ffplugindefs[MAXPLUGINS];
extern int nffglplugindefs;
extern FFGLPluginDef *ffglplugindefs[MAXPLUGINS];

extern FFPluginDef *preplugins[NPREPLUGINS];
extern FFGLViewportStruct fboViewport;
extern FFGLViewportStruct windowViewport;
extern FFGLFBO fbo1;
extern FFGLFBO fbo2;
extern FFGLFBO* fbo_input;
extern FFGLFBO* fbo_output;

extern FFGLPluginDef* post2trails;
extern FFGLPluginDef* post2flip;
extern FFGLPluginDef* post2emptya;
extern FFGLPluginDef* post2emptyb;
extern FFGLPluginDef* post2palette;
extern double curFrameTime;

extern FFGLTextureStruct mapTexture;
extern int fboWidth;
extern int fboHeight;
extern FFGLFBO* fbo_output;
extern FFGLExtensions glExtensions;
extern int	ffWidth;
extern int	ffHeight;

struct CvCapture;
class FFGLPluginInstance;

std::string CopyFFString16(const char *src);
#define FFString CopyFFString16
bool ff_passthru(ProcessOpenGLStruct *pGL);
bool do_ffgl_plugin(FFGLPluginInstance* plugin1,int which);

extern "C" { bool default_setdll(std::string dllpath); }

#define WINDOWS_DLLMAIN_FUNCTION(setdll) \
	BOOL APIENTRY DllMain(HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved) { \
	char dllpath[MAX_PATH]; \
	GetModuleFileNameA((HMODULE)hModule, dllpath, MAX_PATH); \
	\
	if (ul_reason_for_call == DLL_PROCESS_ATTACH ) { \
		if ( ! default_setdll(std::string(dllpath)) ) { \
			return FALSE; \
		} \
	} \
    return TRUE; \
	}

#endif
