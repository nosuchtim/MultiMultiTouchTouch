import java.util.*;

///////////////////////////////////////////////////////////////////////////
//
// A Sprite is a moving graphical entity.
//
// Modify the code in the draw() method to change what it looks like.
//
///////////////////////////////////////////////////////////////////////////

class Sprite {

	float x;
	float y;
	float z;
	int colour;
	int alpha;
	int born;
	float dx;
	float dy;

	Sprite(float x0, float y0, float z0, int c) {
		x = x0;
		y = y0;
		z = z0;
		colour = c;
		alpha = 100;
		born = millis();
		dx = 0;
		dy = 0;
	}

	void setMotion(float newdx, float newdy) {
		dx = newdx;
		dy = newdy;
	}

	void draw() {

		int x0 = (int)(width * x);
		int y0 = (int)(height * y);

		// The size the sprite is dependent
		// on the Z value (depth) of the cursor
		int w = (int)(width * z);
		int h = (int)(height * z);

		fill(colour,alpha);

		rect(x0,y0,w,h);
	}

	void advanceTime() {
		x += dx;
		y += dy;
	}

	boolean isdead() {
		// If it's no longer visible
		if ( x < 0 || x > 1.0 || y < 0 || y > 1.0 ) {
			return true;
		}
		// If it's too old
		if ( (millis() - born) > 1000 ) {
			return true;
		}
		return false;
	}

};
