import java.util.*;

class Area {

	Map<Integer,Cursor> cursors;
	int sidmin;
	int sidmax;
	color colour;
	int channel;

	Area(int mn, int mx) {
		sidmin = mn;
		sidmax = mx;
		cursors = new HashMap<Integer,Cursor>();
		colour = color(255,255,255,255);
		channel = 0;
	}

	void setColor(color c) {
		colour = c;
	}

	void setChannel(int ch) {	// 0-15
		channel = ch;
	}

	synchronized public void clearTouched() {
		for (Integer sid: cursors.keySet()) {
			Cursor c = cursors.get(sid);
			c.touched = false;
		}
	}

	synchronized public void touch(int sid) {
		if ( cursors.containsKey(sid) ) {
			Cursor c = (Cursor) cursors.get(sid);
			c.touched = true;
		}
	}

	synchronized public void setSidPosition(int sid, float x, float y, float z) {
		Cursor c;
		if ( cursors.containsKey(sid) ) {
			c = (Cursor) cursors.get(sid);
			c.setPosition(x,y,z);
			c.cursorDrag();
		} else {
			c = new Cursor(sid,channel);
			c.setPosition(x,y,z);
			cursors.put(sid,c);
			c.cursorDown();
		}
	}


	synchronized public void checkCursorUp() {
		Iterator iter = cursors.entrySet().iterator();
		while ( iter.hasNext() ) {
			Map.Entry pair = (Map.Entry) iter.next();
			Cursor c = (Cursor) pair.getValue();
			int sid = (Integer) pair.getKey();
			if ( ! c.touched ) {
				c.cursorUp();
				iter.remove();
			}
		}
	}

	synchronized public void draw() {
		for (Integer sid: cursors.keySet()) {
			Cursor c = cursors.get(sid);
			c.draw(width,height,colour);
		}
	}
};
