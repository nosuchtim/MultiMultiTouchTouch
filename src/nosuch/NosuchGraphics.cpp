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

#include <Windows.h>
#include <gl/GL.h>

#define _USE_MATH_DEFINES // To get definition of M_PI
#include <math.h>

#include "NosuchDebug.h"
#include "NosuchGraphics.h"
// #include "Sprite.h"

NosuchGraphics::NosuchGraphics() {
	m_filled = false;
	m_stroked = false;
}

void NosuchGraphics::rect(double x, double y, double w, double h) {
	// if ( w != 2.0f || h != 2.0f ) {
	// 	DEBUGPRINT(("Drawing rect xy = %.3f %.3f  wh = %.3f %.3f",x,y,w,h));
	// }
	quad(x,y, x+w,y,  x+w,y+h,  x,y+h);
}
void NosuchGraphics::fill(NosuchColor c, double alpha) {
	m_filled = true;
	m_fill_color = c;
	m_fill_alpha = alpha;
}
void NosuchGraphics::stroke(NosuchColor c, double alpha) {
	// glColor4d(c.r()/255.0f, c.g()/255.0f, c.b()/255.0f, alpha);
	m_stroked = true;
	m_stroke_color = c;
	m_stroke_alpha = alpha;
}
void NosuchGraphics::noStroke() {
	m_stroked = false;
}
void NosuchGraphics::noFill() {
	m_filled = false;
}
void NosuchGraphics::background(int b) {
	DEBUGPRINT(("NosuchGraphics::background!"));
}
void NosuchGraphics::strokeWeight(double w) {
	glLineWidth((GLfloat)w);
}
void NosuchGraphics::rotate(double degrees) {
	glRotated(-degrees,0.0f,0.0f,1.0f);
}
void NosuchGraphics::translate(double x, double y) {
	glTranslated(x,y,0.0f);
}
void NosuchGraphics::scale(double x, double y) {
	glScaled(x,y,1.0f);
}
void NosuchGraphics::quad(double x0, double y0, double x1, double y1, double x2, double y2, double x3, double y3) {
	DEBUGPRINT2(("   Drawing quad = %.3f %.3f, %.3f %.3f, %.3f %.3f, %.3f %.3f",x0,y0,x1,y1,x2,y2,x3,y3));
	if ( m_filled ) {
		glBegin(GL_QUADS);
		NosuchColor c = m_fill_color;
		glColor4d(c.r()/255.0f, c.g()/255.0f, c.b()/255.0f, m_fill_alpha);
		glVertex2d( x0, y0); 
		glVertex2d( x1, y1); 
		glVertex2d( x2, y2); 
		glVertex2d( x3, y3); 
		glEnd();
	}
	if ( m_stroked ) {
		NosuchColor c = m_stroke_color;
		glColor4d(c.r()/255.0f, c.g()/255.0f, c.b()/255.0f, m_stroke_alpha);
		glBegin(GL_LINE_LOOP); 
		glVertex2d( x0, y0); 
		glVertex2d( x1, y1); 
		glVertex2d( x2, y2); 
		glVertex2d( x3, y3); 
		glEnd();
	}
	if ( ! m_filled && ! m_stroked ) {
		DEBUGPRINT(("Hey, quad() called when both m_filled and m_stroked are off!?"));
	}
}
void NosuchGraphics::triangle(double x0, double y0, double x1, double y1, double x2, double y2) {
	DEBUGPRINT2(("Drawing triangle xy0=%.3f,%.3f xy1=%.3f,%.3f xy2=%.3f,%.3f",x0,y0,x1,y1,x2,y2));
	if ( m_filled ) {
		NosuchColor c = m_fill_color;
		glColor4d(c.r()/255.0f, c.g()/255.0f, c.b()/255.0f, m_fill_alpha);
		DEBUGPRINT2(("   fill_color=%d %d %d alpha=%.3f",c.r(),c.g(),c.b(),m_fill_alpha));
		glBegin(GL_TRIANGLE_STRIP); 
		glVertex3d( x0, y0, 0.0f );
		glVertex3d( x1, y1, 0.0f );
		glVertex3d( x2, y2, 0.0f );
		glEnd();
	}
	if ( m_stroked ) {
		NosuchColor c = m_stroke_color;
		glColor4d(c.r()/255.0f, c.g()/255.0f, c.b()/255.0f, m_stroke_alpha);
		DEBUGPRINT2(("   stroke_color=%d %d %d alpha=%.3f",c.r(),c.g(),c.b(),m_stroke_alpha));
		glBegin(GL_LINE_LOOP); 
		glVertex2d( x0, y0); 
		glVertex2d( x1, y1);
		glVertex2d( x2, y2);
		glEnd();
	}
	if ( ! m_filled && ! m_stroked ) {
		DEBUGPRINT(("Hey, triangle() called when both m_filled and m_stroked are off!?"));
	}
}

void NosuchGraphics::line(double x0, double y0, double x1, double y1) {
	if ( m_stroked ) {
		NosuchColor c = m_stroke_color;
		glColor4d(c.r()/255.0f, c.g()/255.0f, c.b()/255.0f, m_stroke_alpha);
		glBegin(GL_LINES); 
		glVertex2d( x0, y0); 
		glVertex2d( x1, y1);
		glEnd();
	} else {
		DEBUGPRINT(("Hey, line() called when m_stroked is off!?"));
	}
}

static double degree2radian(double deg) {
	return 2.0f * (double)M_PI * deg / 360.0f;
}

void NosuchGraphics::ellipse(double x0, double y0, double w, double h) {
	DEBUGPRINT2(("Drawing ellipse xy0=%.3f,%.3f wh=%.3f,%.3f",x0,y0,w,h));
	if ( m_filled ) {
		NosuchColor c = m_fill_color;
		glColor4d(c.r()/255.0f, c.g()/255.0f, c.b()/255.0f, m_fill_alpha);
		DEBUGPRINT2(("   fill_color=%d %d %d alpha=%.3f",c.r(),c.g(),c.b(),m_fill_alpha));
		glBegin(GL_TRIANGLE_FAN);
		double radius = w;
		glVertex2d(x0, y0);
		for ( double degree=0.0f; degree <= 360.0f; degree+=5.0f ) {
			glVertex2d(x0 + sin(degree2radian(degree)) * radius, y0 + cos(degree2radian(degree)) * radius);
		}
		glEnd();
	}
	if ( m_stroked ) {
		NosuchColor c = m_stroke_color;
		glColor4d(c.r()/255.0f, c.g()/255.0f, c.b()/255.0f, m_stroke_alpha);
		DEBUGPRINT2(("   stroke_color=%d %d %d alpha=%.3f",c.r(),c.g(),c.b(),m_stroke_alpha));
		glBegin(GL_LINE_LOOP);
		double radius = w;
		for ( double degree=0.0f; degree <= 360.0f; degree+=5.0f ) {
			glVertex2d(x0 + sin(degree2radian(degree)) * radius, y0 + cos(degree2radian(degree)) * radius);
		}
		glEnd();
	}

	if ( ! m_filled && ! m_stroked ) {
		DEBUGPRINT(("Hey, ellipse() called when both m_filled and m_stroked are off!?"));
	}
}

void NosuchGraphics::polygon(PointMem* points, int npoints) {
	if ( m_filled ) {
		NosuchColor c = m_fill_color;
		glColor4d(c.r()/255.0f, c.g()/255.0f, c.b()/255.0f, m_fill_alpha);
		glBegin(GL_TRIANGLE_FAN);
		glVertex2d(0.0, 0.0);
		for ( int pn=0; pn<npoints; pn++ ) {
			PointMem* p = &points[pn];
			glVertex2d(p->x,p->y);
		}
		glEnd();
	}
	if ( m_stroked ) {
		NosuchColor c = m_stroke_color;
		glColor4d(c.r()/255.0f, c.g()/255.0f, c.b()/255.0f, m_stroke_alpha);
		glBegin(GL_LINE_LOOP);
		for ( int pn=0; pn<npoints; pn++ ) {
			PointMem* p = &points[pn];
			glVertex2d(p->x,p->y);
		}
		glEnd();
	}

	if ( ! m_filled && ! m_stroked ) {
		DEBUGPRINT(("Hey, ellipse() called when both m_filled and m_stroked are off!?"));
	}
}

void NosuchGraphics::popMatrix() {
	glPopMatrix();
}

void NosuchGraphics::pushMatrix() {
	glPushMatrix();
}


