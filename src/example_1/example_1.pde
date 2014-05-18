///////////////////////////////////
//
// This is an example program for
// interpreting the TUIO/OSC messages
// coming from MMTT (MultiMultiTouchTouch).
//
///////////////////////////////////

import java.util.*;
import java.io.StringWriter;

import processing.net.*;
import oscP5.*;
import netP5.*;
import themidibus.*;

OscP5 oscP5;
MidiBus midi;
Area defaultArea;
Areas areas;

void setup() {

	size(800,600);
	background(0);

	areas = new Areas();

	// If you want to send MIDI things to a VST soft synth, use loopMIDI
	// midi = new MidiBus(this, -1, "loopMIDI Port 1");
	midi = new MidiBus(this, -1, "Microsoft GS Wavetable Synth");

	oscP5 = new OscP5(this,3333);

	programchange(0,0);   // channel 1 = acoustic piano
	programchange(1,4);   // channel 2 = electric piano
	programchange(2,11);  // channel 3 = vibes
	programchange(3,80);  // channel 4 = lead (square)
	programchange(4,88);  // channel 5 = pad (new age)

	Area a0 = new Area(0,999);
	a0.setColor(color(255,0,0));
	a0.setChannel(0);
	areas.add("ONE",a0);

	Area a1 = new Area(1000,1999);
	a1.setColor(color(0,255,0));
	a1.setChannel(1);
	areas.add("TWO",a1);

	Area a2 = new Area(2000,2999);
	a2.setColor(color(0,0,255));
	a2.setChannel(2);
	areas.add("THREE",a2);

	Area a3 = new Area(3000,3999);
	a3.setColor(color(255,255,0));
	a3.setChannel(3);
	areas.add("FOUR",a3);

	Area a4 = new Area(4000,4999);
	a4.setColor(color(0,255,255));
	a4.setChannel(4);
	areas.add("FIVE",a4);

	defaultArea = a0;
}

void randomizeprograms() {
	programchange(0,int(random(127)));
	programchange(1,int(random(127)));
	programchange(2,int(random(127)));
	programchange(3,int(random(127)));
	programchange(4,int(random(127)));
}

void keyPressed() {
	randomizeprograms();
}

// Send a MIDI program change message.  Both chan and prog values start at 0.
void programchange(int chan, int prog) {
	midi.sendMessage( 0xc0 | chan, prog );
}

// Draw everything on the screen
void draw() {

	// fade things out gradually
	fill(0,0,0,4);
	rect(0,0,width,height);

	areas.draw();
}

// Incoming osc message are forwarded to the oscEvent method.
void oscEvent(OscMessage m) {

   try {
	Object[] args = m.arguments();

	if (m.checkAddrPattern("/tuio/25Dblb")) {

		String cmd = m.get(0).stringValue();
		if ( cmd.equals("alive") ) {
			int na = m.typetag().length() - 1;

			areas.clearTouched();

			for ( int i=1; i<=na; i++ ) {
				int sid = m.get(i).intValue();
				Area area = areas.findArea(sid);
				if ( area == null ) {
					area = defaultArea;
				}
				area.touch(sid);
			}

			areas.checkCursorUp();

		} else if (cmd.equals("fseq") ) {

		} else if ( cmd.equals("set") ) {

			int sid = m.get(1).intValue();
			float x = m.get(2).floatValue();
			float y = m.get(3).floatValue();
			float z = m.get(4).floatValue();

			Area area = areas.findArea(sid);
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

