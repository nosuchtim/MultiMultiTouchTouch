///////////////////////////////////
//
// This is an example program for
// interpreting the TUIO/OSC messages
// coming from MMTT (MultiMultiTouchTouch).
// This example 
//
///////////////////////////////////

import java.util.*;
import java.io.StringWriter;

import processing.net.*;
import oscP5.*;
import netP5.*;
import themidibus.*;

OscP5 oscP5;
MidiBus myMidi;

void setup() {

	size(800,600);
	background(0);

	myMidi = new MidiBus(this, -1, "Microsoft GS Wavetable Synth");

	// If you want to send MIDI things to a VST soft synth, use loopMIDI
	// myMidi = new MidiBus(this, -1, "loopMIDI Port 1");

	oscP5 = new OscP5(this,3333);

	programchange(0,5);
	programchange(1,20);
	programchange(2,30);
	programchange(3,40);
	programchange(4,50);

	Area a0 = new Area(0,999);
	a0.setColor(color(255,0,0));
	a0.setChannel(0);

	Area a1 = new Area(11000,11999);
	a1.setColor(color(0,255,0));
	a1.setChannel(1);

	Area a2 = new Area(12000,12999);
	a2.setColor(color(0,0,255));
	a2.setChannel(2);

	Area a3 = new Area(13000,13999);
	a3.setColor(color(255,255,0));
	a3.setChannel(3);

	Area a4 = new Area(14000,14999);
	a4.setColor(color(0,255,255));
	a4.setChannel(4);

	defaultArea = a0;
}

// Send a MIDI program change message.  Both chan and prog values start at 0.
void programchange(int chan, int prog) {
	myMidi.sendMessage( 0xc0 | chan, prog );
}

// Draw everything on the screen
void draw() {

	// fade things out gradually
	fill(0,0,0,128);
	rect(0,0,width,height);

	Area.draw();
	for (String a: areas.keySet()) {
		Area area = areas.get(a); 
		area.draw();
	}
}

// Incoming osc message are forwarded to the oscEvent method.
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
				if ( area == null ) {
					area = defaultArea;
				}
				area.touch(sid);
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
			if ( area == null ) {
				area = defaultArea;
			}
			area.setSidPosition(sid,x,y,z);
		}
	}
    } catch (Exception e) {
	println("HEY!! EXCEPTION in oscEvent! msg="+ExceptionString(e));
    }
}
 
String ExceptionString(Exception e) {
	StringWriter sw = new StringWriter();
	PrintWriter pw = new PrintWriter(sw);
	e.printStackTrace(pw);
	return sw.toString();
}

