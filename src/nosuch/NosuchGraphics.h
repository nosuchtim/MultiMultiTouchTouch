#ifndef NOSUCHGRAPHICS_H
#define NOSUCHGRAPHICS_H

#include <math.h>
#include <float.h>

#include "NosuchColor.h"

class Sprite;

#define RADIAN2DEGREE(r) ((r) * 360.0 / (2.0 * (double)M_PI))
#define DEGREE2RADIAN(d) (((d) * 2.0 * (double)M_PI) / 360.0 )

// #define CHECK_VECTOR
#ifdef CHECK_VECTOR_SANITY
void checkVectorSanity(NosuchVector v) {
	if ( _isnan(v.x) || _isnan(v.y) ) {
		DEBUGPRINT(("checkVectorSanity found NaN!"));
	}
	if ( v.x > 10.0 || v.y > 10.0 ) {
		DEBUGPRINT(("checkVectorSanity found > 10.0!"));
	}
}
#else
#define checkVectorSanity(v)
#endif

class NosuchVector {
public:
	NosuchVector() {
		// set(FLT_MAX,FLT_MAX,FLT_MAX);
		set(0.0,0.0);
	}
	NosuchVector(double xx, double yy) {
		set(xx,yy);
	};
	void set(double xx, double yy) {
		x = xx;
		y = yy;
	}
	bool isnull() {
		return ( x == FLT_MAX && y == FLT_MAX );
	}
	bool isequal(NosuchVector p) {
		return ( x == p.x && y == p.y );
	}
	NosuchVector sub(NosuchVector v) {
		return NosuchVector(x-v.x,y-v.y);
	}
	NosuchVector add(NosuchVector v) {
		return NosuchVector(x+v.x,y+v.y);
	}
	double mag() {
		return sqrt( (x*x) + (y*y) );
	}
	NosuchVector normalize() {
		double leng = mag();
		return NosuchVector(x/leng, y/leng);
	}
	NosuchVector mult(double m) {
		return NosuchVector(x*m,y*m);
	}
	NosuchVector rotate(double radians, NosuchVector about = NosuchVector(0.0f,0.0f) ) {
		double c, s;
		c = cos(radians);
		s = sin(radians);
		x -= about.x;
		y -= about.y;
		double newx = x * c - y * s;
		double newy = x * s + y * c;
		newx += about.x;
		newy += about.y;
		return NosuchVector(newx,newy);
	}
	double heading() {
        return -atan2(-y, x);
	}

	double x;
	double y;
};

class NosuchPos {
public:
	NosuchPos() {
		set(0.0,0.0,0.0);
	}
	NosuchPos(double xx, double yy, double zz) {
		set(xx,yy,zz);
	};
	void set(double xx, double yy, double zz) {
		x = xx;
		y = yy;
		z = zz;
	}
	double mag() {
		return sqrt( (x*x) + (y*y) + (z*z) );
	}
	NosuchPos normalize() {
		double leng = mag();
		return NosuchPos(x/leng, y/leng, z/leng);
	}
	NosuchPos add(NosuchPos p) {
		return NosuchPos(x+p.x,y+p.y,z+p.z);
	}
	NosuchPos mult(double m) {
		return NosuchPos(x*m,y*m,z*m);
	}

	double x;
	double y;
	double z;
};

#if 0
typedef struct VideoPixel24bitTag {
	BYTE blue;
	BYTE green;
	BYTE red;
} VideoPixel24bit;
#endif

typedef struct PointMem {
	float x;
	float y;
	float z;
} PointMem;

class NosuchGraphics {
public:
	NosuchGraphics();

	static int NextSeq();

	bool m_filled;
	NosuchColor m_fill_color;
	double m_fill_alpha;
	bool m_stroked;
	NosuchColor m_stroke_color;
	double m_stroke_alpha;

	void fill(NosuchColor c, double alpha);
	void noFill();
	void stroke(NosuchColor c, double alpha);
	void noStroke();
	void strokeWeight(double w);
	void background(int);
	void rect(double x, double y, double width, double height);
	void pushMatrix();
	void popMatrix();
	void translate(double x, double y);
	void scale(double x, double y);
	void rotate(double degrees);
	void line(double x0, double y0, double x1, double y1);
	void triangle(double x0, double y0, double x1, double y1, double x2, double y2);
	void quad(double x0, double y0, double x1, double y1, double x2, double y2, double x3, double y3);
	void ellipse(double x0, double y0, double w, double h);
	void polygon(PointMem* p, int npoints);

private:
	static int nextseq;
};

#endif
