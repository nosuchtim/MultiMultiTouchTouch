/*
	Copyright (c) 2011-2013 Tim Thompson <me@timthompson.com>

	Permission is hereby granted, free of charge, to any person obtaining
	a copy of this software and associated documentation files
	(the "Software"), to deal in the Software without restriction,
	including without limitation the rights to use, copy, modify, merge,
	publish, distribute, sublicense, and/or sell copies of the Software,
	and to permit persons to whom the Software is furnished to do so,
	subject to the following conditions:

	The above copyright notice and this permission notice shall be
	included in all copies or substantial portions of the Software.

	Any person wishing to distribute modifications to the Software is
	requested to send the modifications to the original developer so that
	they can be incorporated into the canonical version.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
	EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
	MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
	IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
	ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
	CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
	WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "NosuchUtil.h"
#include "NosuchException.h"
#include "NosuchMidi.h"
#include "midifile.h"
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>

static char *ReadableMidiPitches[128];
static char *ReadableCanonic[12] = {
	"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};
static bool ReadableNotesInitialized = false;

bool NosuchDebugMidiAll = false;
bool NosuchDebugMidiNotes = false;

int QuarterNoteClicks = 96;

char* ReadableMidiPitch(int p) {
	if ( ! ReadableNotesInitialized ) {
		for ( int n=0; n<128; n++ ) {
			int canonic = n % 12;
			int octave = (n / 12) - 2;
			std::string s = NosuchSnprintf("%s%d",ReadableCanonic[canonic],octave);
			ReadableMidiPitches[n] = _strdup(s.c_str());
		}
		ReadableNotesInitialized = true;
	}
	return ReadableMidiPitches[p];
}

MidiMsg* MidiMsg::make(int msg) {
	int status = Pm_MessageStatus(msg);
	int command = status & 0xf0;
	int chan = 1 + (status & 0x0f);
	int byte1 = Pm_MessageData1(msg);
	int byte2 = Pm_MessageData2(msg);
	switch (command) {
	case MIDI_NOTE_ON:
		return MidiNoteOn::make(chan,byte1,byte2);
	case MIDI_NOTE_OFF:
		return MidiNoteOff::make(chan,byte1,byte2);
	case MIDI_CONTROL:
		return MidiController::make(chan,byte1,byte2);
	case MIDI_PROGRAM:
		return MidiProgramChange::make(chan,byte1);
	default:
		DEBUGPRINT(("Unhandled command in MidiMsg::make!  cmd=0x%02x",command));
		return NULL;
	}
}

static MidiPhrase* MidiFilePhrase = NULL;
static std::ifstream Mf_input_stream;

int my_Mf_insert(MidiMsg* m) {
	MidiFilePhrase->insert(m,Mf_currtime);
	return 0;
}

int my_Mf_noteon(int ch,int p, int v) {
	DEBUGPRINT1(("my_Mf_noteon was called, p=%d v=%d",p,v));
	ch++; // In MidiMsg, channels are 1-16
	if ( v == 0 ) {
		return my_Mf_insert(MidiNoteOff::make(ch,p,v));
	} else {
		return my_Mf_insert(MidiNoteOn::make(ch,p,v));
	}
}

int my_Mf_noteoff(int ch, int p, int v) {
	DEBUGPRINT1(("my_Mf_noteoff was called, p=%d v=%d",p,v));
	ch++; // In MidiMsg, channels are 1-16
	return my_Mf_insert(MidiNoteOff::make(ch,p,v));
}

int my_Mf_pressure(int ch, int p, int v) {
	DEBUGPRINT1(("my_Mf_pressure was called, p=%d v=%d",p,v));
	ch++; // In MidiMsg, channels are 1-16
	return my_Mf_insert(MidiPressure::make(ch,p,v));
}

int my_Mf_controller(int ch, int p, int v) {
	DEBUGPRINT1(("my_Mf_controller was called, p=%d v=%d",p,v));
	ch++; // In MidiMsg, channels are 1-16
	return my_Mf_insert(MidiController::make(ch,p,v));
}

int my_Mf_pitchbend(int ch, int lsb, int msb) {
	ch++; // In MidiMsg, channels are 1-16
	int v = lsb | (msb<<7);		// 7 bits from lsb, 7 bits from msb
	DEBUGPRINT1(("my_Mf_pitchbend was called, v=%d",v));
	return my_Mf_insert(MidiPitchBend::make(ch,v));
}

int my_Mf_program(int ch, int p) {
	ch++; // In MidiMsg, channels are 1-16
	p++;  // In MidiProgramChange, program values are 1-128
	DEBUGPRINT1(("my_Mf_program was called, p=%d",p));
	return my_Mf_insert(MidiProgramChange::make(ch,p));
}

int my_Mf_chanpressure(int ch, int p) {
	ch++; // In MidiMsg, channels are 1-16
	DEBUGPRINT1(("my_Mf_chanpressure was called, p=%d",p));
	return my_Mf_insert(MidiChanPressure::make(ch,p));
}

int my_Mf_sysex(int msgleng,char* p) {
	DEBUGPRINT1(("my_Mf_sysex was called, msgleng=%d",msgleng));
	return 0;
}

int my_Mf_getc() {
	int c = Mf_input_stream.get();
	if ( Mf_input_stream.eof() ) {
		return EOF;
	} else {
		return c;
	}
}

MidiPhrase*
newMidiPhraseFromFile(std::string fname) {

	// XXX - should really lock something here, since it uses globals
	Mf_noteon = my_Mf_noteon;
	Mf_noteoff = my_Mf_noteoff;
	Mf_getc = my_Mf_getc;
	Mf_pressure = my_Mf_pressure;
	Mf_controller = my_Mf_controller;
	Mf_pitchbend = my_Mf_pitchbend;
	Mf_program = my_Mf_program;
	Mf_chanpressure = my_Mf_chanpressure;
	Mf_sysex = my_Mf_sysex;

	std::string err = "";

	Mf_input_stream.open(fname.c_str(),std::ios::binary);
	if ( ! Mf_input_stream.good() ) {
		return NULL;
	}
	MidiFilePhrase = new MidiPhrase();
	err = mfread();
	Mf_input_stream.close();
#if 0
	DEBUGPRINT(("MidiPhraseFromFile = %s",
	 	MidiFilePhrase->DebugString().c_str()));
	DEBUGPRINT(("MidiPhraseFromFile = %s",
		MidiFilePhrase->SummaryString().c_str()));
#endif
	if ( err == "" ) {
		return MidiFilePhrase;	// caller is responsible for deleting
	} else {
		DEBUGPRINT(("Error in reading midifile: %s",err.c_str()));
		delete MidiFilePhrase;
		MidiFilePhrase = NULL;
		return NULL;
	}
}

void MidiPhrase::insert(MidiMsg* msg, click_t click) {  // takes ownership
	MidiPhraseUnit* pu = new MidiPhraseUnit(msg,click);
	if ( first == NULL ) {
		first = last = pu;
	} else if ( click >= last->click ) {
		// quick append to end of phrase
		last->next = pu;
		last = pu;
	} else {
		MidiPhraseUnit* pos = first;
		MidiPhraseUnit* prev = NULL;
		while ( pos != NULL && click >= pos->click ) {
			prev = pos;
			pos = pos->next;
		}
		if ( pos == NULL ) {
			// It's at the end
			last->next = pu;
			last = pu;
		} else if ( prev == NULL ) {
			// It's at the beginning
			pu->next = first;
			first = pu;
		} else {
			pu->next = pos;
			prev->next = pu;
		}
	}
}
