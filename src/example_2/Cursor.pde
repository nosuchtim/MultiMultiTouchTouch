import java.util.*;

class Cursor {

	int sid;
	float x;
	float y;
	float z;
	float x0;
	float y0;
	float z0;
	boolean touched;
	int alpha;

	public int pitch;
	public int channel;

	public Cursor(int sid_, int channel_) {
		sid = sid_;
		channel = channel_;
		setPosition(0.0f,0.0f,0.0f);
		touched = false;
		alpha = 100;
		x0 = x;
		y0 = y;
		z0 = z;
		pitch = -1;
	}

	public void setPosition(float x_, float y_, float z_) {
		x = x_;
		y = y_;
		z = z_;
	}

	public void resetPosition0() {
		x0 = x;
		y0 = y;
		z0 = z;
	}

	public float distFromPosition0() {
		float dist = sqrt( (x-x0)*(x-x0) + (y-y0)*(y-y0) + (z-z0)*(z-z0)  );
		return dist;
	}
};

