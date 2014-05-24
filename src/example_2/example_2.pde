///////////////////////////////////////////////////////////////////////////
//
// This is an example program for interpreting the TUIO/OSC messages
// coming from MMTT (MultiMultiTouchTouch).
//
// This is a little more musical version of example_1
//
// Notes/Sprites are only generated if a Cursor moves
// more than a certain amount.
//
// Pressing ' ' (space bar) changes the sounds.
//
// Pressing '0', '1', '2', ... '9' changes the scale being used.
//
// Pressing 'a', 'b', 'c', ... 'g' changes the key (pitch offset) being used.
//
///////////////////////////////////////////////////////////////////////////

import java.util.*;
import java.io.StringWriter;

import processing.net.*;
import oscP5.*;
import netP5.*;
import themidibus.*;

OscP5 oscP5;
MidiBus midi;
Areas areas;
Sprites sprites;
Map colorOfArea;
Map channelOfArea;
Map scales;
Map keyoffsets;
Scale currentScale;
int currentKeyoffset;

///////////////////////////////////////////////////////////////////////////
// The setup() and draw() methods are standard Processing methods.
// setup() gets called only once, but draw() gets called repeatedly.
///////////////////////////////////////////////////////////////////////////

void setup() {

	size(800,600);
	background(0);

	sprites = new Sprites();

	// If you want to send MIDI things to a VST soft synth, use loopMIDI.
	// midi = new MidiBus(this, -1, "loopMIDI Port 1");

	midi = new MidiBus(this, -1, "Microsoft GS Wavetable Synth");

	// OscP5 will call oscEvent() when things are received
	oscP5 = new OscP5(this,3333);

	initializeScales();

	initializePrograms();

	initializeAreas();
}

void draw() {
	background(0);		// background is black
	sprites.draw();
	sprites.advanceTime();
}

///////////////////////////////////////////////////////////////////////////
// These methods are called for Cursor events (generated in the Area class)
///////////////////////////////////////////////////////////////////////////

void cursorDownEvent(Area a, Cursor c) {

	// Generate a NOTEON, and save the pitch in the Cursor,
	// so that we can make sure the NOTEOFF uses the same pitch
	c.pitch = pitchOf(c);
	midi.sendNoteOn(a.channel, c.pitch, velocityOf(c));

	// Generate a Sprite using the cursor position
	Sprite s = new Sprite(c.x,c.y,c.z,a.colour);

	// The motion of the sprite is random in each direction
	s.setMotion( random(-0.005,0.005), random(-0.005,0.005) );

	sprites.add(s);
}

void cursorDragEvent(Area a, Cursor c) {

	// We only do something if the Cursor
	// has moved more than a certain amount

	if ( c.distFromPosition0() > 0.05 ) {

		c.resetPosition0();

		// uses pitch saved in Cursor
		midi.sendNoteOff(a.channel, c.pitch, 0);

		cursorDownEvent(a,c);   // then do the same thing as a cursorDown
	}
}

void cursorUpEvent(Area a, Cursor c) {
	midi.sendNoteOff(a.channel, c.pitch, 0);
}

///////////////////////////////////////////////////////////////////////////
// These methods compute the pitch and velocity (normally volume) for the
// MIDI notes generated from a Cursor.
///////////////////////////////////////////////////////////////////////////

int pitchOf(Cursor c) {
	int p = (int)(c.x * 127);  // from 0 to 127
	p = currentScale.closestTo(p);
	return p + currentKeyoffset;
}

int velocityOf(Cursor c) {
	int v = (int)((c.z*3) * 127);   // the factor of 3 increases the volume
	if ( v > 127 ) {
		v = 127;
	}
	return v;
}

///////////////////////////////////////////////////////////////////////////
// Set up the Areas that we're going to respond to.
// Each Area "listens" for a range of SID (session ID) values.
// Each Area is mapped to a different color and MIDI channel.
// For example, when Sprites are created by a Cursor in a particular area,
// the color of the Sprite will be the color for that area.  Likewise,
// MIDI notes created by a Cursor will be on the MIDI channel for that area.
///////////////////////////////////////////////////////////////////////////

void initializeAreas() {

	areas = new Areas();
	colorOfArea = new HashMap();
	channelOfArea = new HashMap();

	Area a1 = new Area(1000,1999);	// listen for SIDs from 1000-1999
	Area a2 = new Area(2000,2999);
	Area a3 = new Area(3000,3999);
	Area a4 = new Area(4000,4999);

	a1.colour = color(255,255,0);
	a2.colour = color(255,0,255);
	a3.colour = color(0,255,0);
	a4.colour = color(0,255,255);

	a1.channel = 0;
	a2.channel = 1;
	a3.channel = 2;
	a4.channel = 3;

	areas.add("ONE",a1);
	areas.add("TWO",a2);
	areas.add("THREE",a3);
	areas.add("FOUR",a4);
}

void initializeScales() {

	scales = new HashMap();

	scales.put('0', new Scale("chromatic",0,1,2,3,4,5,6,7,8,9,10,11));
	scales.put('1', new Scale("newage",0,3,5,7,10));
	scales.put('2', new Scale("arabian",0,1,4,5,7,8,10));
	scales.put('3', new Scale("harminor",0,2,3,5,7,8,11));
	scales.put('4', new Scale("melminor",0,2,3,5,7,9,11));
	scales.put('5', new Scale("aeolian",0,2,3,5,7,8,10));
	scales.put('6', new Scale("dorian",0,2,3,5,7,9,10));
	scales.put('7', new Scale("ionian",0,2,4,5,7,9,11));
	scales.put('8', new Scale("octaves",0));
	scales.put('9', new Scale("phrygian",0,1,3,5,7,8,10));

	scales.put('-', new Scale("lydian",0,2,4,6,7,9,11));
	scales.put('=', new Scale("mixolydian",0,2,4,5,7,9,10));
	scales.put('`', new Scale("locrian",0,1,3,5,6,8,10));

	currentScale = (Scale) scales.get('1');

	keyoffsets = new HashMap();
	keyoffsets.put('a',-3);
	keyoffsets.put('A',-2);
	keyoffsets.put('b',-1);
	keyoffsets.put('c',0);
	keyoffsets.put('C',1);
	keyoffsets.put('d',2);
	keyoffsets.put('D',3);
	keyoffsets.put('e',4);
	keyoffsets.put('f',5);
	keyoffsets.put('F',6);
	keyoffsets.put('g',7);
	keyoffsets.put('G',8);

	currentKeyoffset = (Integer) keyoffsets.get('c');
}

///////////////////////////////////////////////////////////////////////////
// Programs are MIDI Programs.  Values go from 0 to 127.
// Each program number is a different patch or sound.
// If you're driving a General MIDI synth, the patch numbers are
// standardized (e.g. patch number 4 is an electric piano).
///////////////////////////////////////////////////////////////////////////

void initializePrograms() {
	programchange(0,0);	// channel 1 = acoustic piano
	programchange(1,107);	// channel 2 = koto
	programchange(2,11);	// channel 3 = vibes
	programchange(3,80);	// channel 4 = lead (square)
}

void randomizePrograms() {
	programchange(0,int(random(127)));
	programchange(1,int(random(127)));
	programchange(2,int(random(127)));
	programchange(3,int(random(127)));
}

///////////////////////////////////////////////////////////////////////////
// When you press a key on your QWERTY keyboard, keyPressed() is called.
///////////////////////////////////////////////////////////////////////////

void keyPressed() {

	// When the space bar is hit, randomize the MIDI program sounds.

	// Key offsets (transpositions) are controlled
	// by pressing 'a', 'b', 'c', etc.
	// 'A' is a-sharp, 'C' is c-sharp, etc.

	// Scales are controlled by pressing '0', '1', '2', etc.
	// See initializeScales() for the mappings.

	if ( key == ' ' ) {
		println("Changing sounds...");
		randomizePrograms();
	} else if ( scales.get(key) != null ) {
		currentScale = (Scale) scales.get(key);
		println("Changing scale to ",currentScale.name);
	} else if ( keyoffsets.get(key) != null ) {
		currentKeyoffset = (Integer) keyoffsets.get(key);
		println("Changing key to ",key);
	}
}

///////////////////////////////////////////////////////////////////////////
// Send a MIDI program change message.  Both chan and prog values start at 0.
///////////////////////////////////////////////////////////////////////////

void programchange(int chan, int prog) {
	midi.sendMessage( 0xc0 | chan, prog );
}

///////////////////////////////////////////////////////////////////////////
// Incoming osc message are forwarded to the oscEvent method.
///////////////////////////////////////////////////////////////////////////

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
				if ( area != null ) {
					area.touch(sid);
				}
			}

			areas.checkCursorUp();

		} else if (cmd.equals("fseq") ) {

		} else if ( cmd.equals("set") ) {

			int sid = m.get(1).intValue();
			float x = m.get(2).floatValue();
			float y = m.get(3).floatValue();
			float z = m.get(4).floatValue();

			Area area = areas.findArea(sid);
			if ( area != null ) {
				area.setSidPosition(sid,x,y,z);
			}
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

