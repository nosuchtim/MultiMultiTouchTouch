import java.util.*;

class Cursor {

	int sid;
	float x;
	float y;
	float z;
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
	}

	public void setPosition(float x_, float y_, float z_) {
		x = x_;
		y = 1.0f - y_;
		z = z_;
	}
};

