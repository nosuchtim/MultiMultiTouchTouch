import java.util.*;

class Cursor {

	int sid;
	float x;
	float y;
	float z;
	boolean touched;
	int alpha;
	int pitch;
	int velocity;
	int channel;

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

	public void draw(int width, int height, color c) {
		int x0 = (int)(width * x);
		int y0 = (int)(height * y);
		int w = (int)(width * z);
		int h = (int)(height * z);
		fill(c,alpha);
		rect(x0,y0,w,h);
	}

	public void setPitchEtc() {
		pitch = (int)(x * 127);
		velocity = (int)((z*3) * 127);
		if ( velocity > 127 ) {
			velocity = 127;
		}
	}

	public void cursorDown() {
		setPitchEtc();
		midi.sendNoteOn(channel,pitch,velocity);
	}
	public void cursorDrag() {
		midi.sendNoteOff(channel,pitch,velocity);
		setPitchEtc();
		midi.sendNoteOn(channel,pitch,velocity);
	}
	public void cursorUp() {
		midi.sendNoteOff(channel,pitch,velocity);
	}
};

