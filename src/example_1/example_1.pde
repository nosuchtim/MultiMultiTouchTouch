import java.util.*;
import java.io.StringWriter;

import processing.net.*;
import oscP5.*;
import netP5.*;
import themidibus.*;

OscP5 oscP5;
MidiBus myMidi;
Map<String,Area> areas;

void setup() {

	size(800,600);
	background(0);

	myMidi = new MidiBus(this, -1, "loopMIDI Port 1");
	oscP5 = new OscP5(this,3333);

	areas = new HashMap<String,Area>();

	Area a0 = new Area(0,1000);
	a0.setColor(color(255,0,0));
	a0.setChannel(0);

	Area a1 = new Area(1001,1999);
	a1.setColor(color(0,255,0));
	a1.setChannel(1);

	areas.put("A0",a0);
	areas.put("A1",a1);
}

void draw() {

	// fade things out gradually
	fill(0,0,0,10);
	rect(0,0,width,height);

	for (String a: areas.keySet()) {
		Area area = areas.get(a); 
		area.draw();
	}
}

// incoming osc message are forwarded to the oscEvent method.
void oscEvent(OscMessage m) {

   try {
	Object[] args = m.arguments();

	if (m.checkAddrPattern("/tuio/25Dblb")) {

		String cmd = m.get(0).stringValue();
		if ( cmd.equals("alive") ) {
			int na = m.typetag().length() - 1;

			for (String a: areas.keySet()) {
				Area area = areas.get(a); 
				area.clear_touched();
			}

			for ( int i=1; i<=na; i++ ) {
				int sid = m.get(i).intValue();
				Area area = findArea(sid);
				if ( area != null ) {
					area.touch(sid);
				} else {
					println("No area for sid="+sid+"?");
				}
			}

			for (String a: areas.keySet()) {
				Area area = areas.get(a); 
				area.checkCursorUp();
			}

		} else if (cmd.equals("fseq") ) {

		} else if ( cmd.equals("set") ) {

			int sid = m.get(1).intValue();
			float x = m.get(2).floatValue();
			float y = m.get(3).floatValue();
			float z = m.get(4).floatValue();

			Area area = findArea(sid);
			if ( area != null ) {
				area.setSidPosition(sid,x,y,z);
			} else {
				println("No area for sid="+sid+"?");
			}
		}
	}
    } catch (Exception e) {
	println("HEY!! EXCEPTION in oscEvent! msg="+ExceptionString(e));
    }
}
 
Area findArea(int sid) {
	for (String a: areas.keySet()) {
		Area area = areas.get(a); 
		if ( sid >= area.sidmin && sid <= area.sidmax ) {
			return area;
		}
	}
	return null;
};

String ExceptionString(Exception e) {
	StringWriter sw = new StringWriter();
	PrintWriter pw = new PrintWriter(sw);
	e.printStackTrace(pw);
	return sw.toString();
}

