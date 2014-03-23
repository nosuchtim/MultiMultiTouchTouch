#ifndef NOSUCHMIDI_H
#define NOSUCHMIDI_H

#include "portmidi.h"
#include "NosuchScheduler.h"
#include "NosuchException.h"

#define MIDI_CLOCK      0xf8
#define MIDI_ACTIVE     0xfe
#define MIDI_STATUS_MASK 0x80
#define MIDI_SYSEX      0xf0
#define MIDI_EOX        0xf7
#define MIDI_START      0xFA
#define MIDI_STOP       0xFC
#define MIDI_CONTINUE   0xFB
#define MIDI_F9         0xF9
#define MIDI_FD         0xFD
#define MIDI_RESET      0xFF
#define MIDI_NOTE_ON    0x90
#define MIDI_NOTE_OFF   0x80
#define MIDI_CHANNEL_AT 0xD0
#define MIDI_POLY_AT    0xA0
#define MIDI_PROGRAM    0xC0
#define MIDI_CONTROL    0xB0
#define MIDI_PITCHBEND  0xE0
#define MIDI_MTC        0xF1
#define MIDI_SONGPOS    0xF2
#define MIDI_SONGSEL    0xF3
#define MIDI_TUNE       0xF6

// Other defines that are like MIDI_* ,
// but the values are arbitrary, not status nibbles.
#define MIDI_ALL	0x100
#define MIDI_NOTE	0x101

extern int QuarterNoteClicks;
extern bool NosuchDebugMidiAll;
extern bool NosuchDebugMidiNotes;
// #define QuarterNoteClicks 96

typedef long MidiTimestamp;

#define NO_VALUE -1

char* ReadableMidiPitch(int pitch);

class MidiFilter {
public:
	MidiFilter() {
		msgtype = MIDI_ALL;
		chan = 0;
	}
	MidiFilter(int mt, int ch) {
		msgtype = mt;
		chan = ch;
	}

	int msgtype;
	int chan;  // If 0, match all channels
};

class MidiMsg {
public:
	MidiMsg() {
		DEBUGPRINT2(("MidiMsg constructor!"));
	}
	virtual ~MidiMsg() {
		DEBUGPRINT2(("MidiMsg destructor! this=%d",this));
	}
	virtual std::string DebugString() = 0;
	virtual PmMessage PortMidiMessage() = 0;
	virtual PmMessage PortMidiMessageOff() { return 0; }
	virtual int MidiType() { return -1; }
	virtual int Channel() { return -1; }
	virtual int Pitch() { return -1; }
	virtual void Transpose(int dp) { }
	virtual int Velocity() { return -1; }
	virtual int Controller() { return -1; }
	virtual int Value(int val = NO_VALUE) { return NO_VALUE; }
	bool isSameAs(MidiMsg* m) {
		NosuchAssert(m);
		switch (MidiType()) {
		case MIDI_NOTE_ON:
		case MIDI_NOTE_OFF:
			if ( MidiType() == m->MidiType()
				&& Channel() == m->Channel()
				&& Pitch() == m->Pitch()
				&& Velocity() == m->Velocity() )
				return true;
			break;
		default:
			DEBUGPRINT(("MidiMsg::isSameAs not implemented for MidiType=%d",MidiType()));
			break;
		}
		return false;
	}
	virtual MidiMsg* clone() {
		DEBUGPRINT(("Unable to clone MidiMsg of type %d, returning NULL",MidiType()));
		return NULL;
	}
	static MidiMsg* make(int msg);

	bool matches(MidiFilter mf) {
		if ( Channel() > 0 ) {
			// Channel message
			if (mf.chan == 0 || mf.chan == Channel() ) {
				return true;
			}
		} else {
			// non-Channel message
		}
		return false;
	}
};


class ChanMsg : public MidiMsg {
public:
	ChanMsg(int ch) : MidiMsg() {
		NosuchAssert(ch>=1 && ch<=16);
		DEBUGPRINT2(("ChanMsg constructor"));
		_chan = ch;
	}
	virtual ~ChanMsg() {
		DEBUGPRINT2(("ChanMsg destructor"));
	}
	virtual std::string DebugString() = 0;
	virtual PmMessage PortMidiMessage() = 0;
	virtual PmMessage PortMidiMessageOff() { return 0; }
	virtual int MidiType() { return -1; }
	virtual int Pitch() { return -1; }
	virtual int Velocity() { return -1; }
	virtual int Controller() { return -1; }
	virtual int Value(int v = NO_VALUE) { return NO_VALUE; }
	int Channel() { return _chan; }
protected:
	int _chan;   // 1-based
};

class MidiNoteOff : public ChanMsg {
public:
	static MidiNoteOff* make(int ch, int p, int v) {
		MidiNoteOff* m = new MidiNoteOff(ch,p,v);
		DEBUGPRINT2(("MidiNoteOff::make m=%d",m));
		return m;
	};
	std::string DebugString() {
		return NoteString();
	}
	std::string NoteString() {
		return NosuchSnprintf("-p%dc%dv%d",_pitch,_chan,_velocity);
	}
	PmMessage PortMidiMessage() {
		return Pm_Message(0x80 | (_chan-1), _pitch, _velocity);
	}
	int MidiType() { return MIDI_NOTE_OFF; }
	int Pitch() { return _pitch; }
	int Velocity() { return _velocity; }
	MidiNoteOff* clone() {
		MidiNoteOff* newm = MidiNoteOff::make(Channel(),Pitch(),Velocity());
		return newm;
	};
	void Transpose(int dp) {
		MidiNoteOff* m = (MidiNoteOff*)this;
		m->_pitch += dp;
	}
private:
	MidiNoteOff(int ch, int p, int v) : ChanMsg(ch) {
		_pitch = p;
		_velocity = v;
	};
	int _pitch;
	int _velocity;
};

class MidiNoteOn : public ChanMsg {
public:
	static MidiNoteOn* make(int ch, int p, int v) {
		MidiNoteOn* m = new MidiNoteOn(ch,p,v);
		DEBUGPRINT2(("MidiNoteOn::make m=%d",m));
		return m;
	}
	MidiNoteOff* makenoteoff() {
		MidiNoteOff* newm = MidiNoteOff::make(Channel(),Pitch(),Velocity());
		return newm;
	}
	MidiNoteOn* clone() {
		MidiNoteOn* newm = MidiNoteOn::make(Channel(),Pitch(),Velocity());
		return newm;
	};
	void Transpose(int dp) {
		MidiNoteOn* m = (MidiNoteOn*)this;
		m->_pitch += dp;
	}
	PmMessage PortMidiMessage() {
		return Pm_Message(0x90 | (_chan-1), _pitch, _velocity);
	}
	PmMessage PortMidiMessageOff() {
		return Pm_Message(0x80 | (_chan-1), _pitch, 0);
	}
	std::string DebugString() {
		return NoteString();
	}
	std::string NoteString() {
		return NosuchSnprintf("+p%dc%dv%d",_pitch,_chan,_velocity);
	}
	int MidiType() { return MIDI_NOTE_ON; }
	int Pitch() { return _pitch; }
	int Velocity() { return _velocity; }
	int Velocity(int v) { _velocity = v; return _velocity; }
#if 0
	~MidiNoteOn() {
		DEBUGPRINT2(("MidiNoteOn destructor"));
	}
#endif
private:
	MidiNoteOn(int ch, int p, int v) : ChanMsg(ch) {
		DEBUGPRINT2(("MidiNoteOn constructor"));
		_pitch = p;
		_velocity = v;
	};
	int _pitch;
	int _velocity;
};

class MidiController : public ChanMsg {
public:
	static MidiController* make(int ch, int ctrl, int v) {
		MidiController* m = new MidiController(ch,ctrl,v);
		return m;
	};
	std::string DebugString() {
		return NosuchSnprintf("Controller ch=%d ct=%d v=%d",_chan,_controller,_value);
	}
	PmMessage PortMidiMessage() {
		return Pm_Message(0xb0 | (_chan-1), _controller, _value);
	}
	int MidiType() { return MIDI_CONTROL; }
	int Controller() { return _controller; }
	int Value(int v = NO_VALUE) {
		if ( v >= 0 ) {
			NosuchAssert(v <= 127);
			_value = v;
		}
		return _value;
	}
	MidiController* clone() {
		MidiController* newm = MidiController::make(Channel(),Controller(),Value());
		return newm;
	};
private:
	MidiController(int ch, int ctrl, int v) : ChanMsg(ch) {
		_controller = ctrl;
		_value = v;
	};
	int _controller;
	int _value;
};

class MidiChanPressure : public ChanMsg {
public:
	static MidiChanPressure* make(int ch, int v) {
		MidiChanPressure* m = new MidiChanPressure(ch,v);
		return m;
	};
	std::string DebugString() {
		return NosuchSnprintf("ChanPressure ch=%d v=%d",_chan,_value);
	}
	PmMessage PortMidiMessage() {
		return Pm_Message(0xb0 | (_chan-1), _value, 0);
	}
	int MidiType() { return MIDI_CHANNEL_AT; }
	int Value(int v = NO_VALUE) {
		if ( v >= 0 ) {
			NosuchAssert(v <= 127);
			_value = v;
		}
		return _value;
	}
	MidiChanPressure* clone() {
		MidiChanPressure* newm = MidiChanPressure::make(Channel(),Value());
		return newm;
	};
private:
	MidiChanPressure(int ch, int v) : ChanMsg(ch) {
		_value = v;
	};
	int _value;
};

class MidiPressure : public ChanMsg {
public:
	static MidiPressure* make(int ch, int pitch, int v) {
		MidiPressure* m = new MidiPressure(ch,pitch,v);
		return m;
	};
	std::string DebugString() {
		return NosuchSnprintf("Pressure ch=%d pitch=%d v=%d",_chan,_pitch,_value);
	}
	PmMessage PortMidiMessage() {
		return Pm_Message(0xb0 | (_chan-1), _pitch, _value);
	}
	int MidiType() { return MIDI_POLY_AT; }
	int Pitch() { return _pitch; }
	int Value(int v = NO_VALUE) {
		if ( v >= 0 ) {
			NosuchAssert(v <= 127);
			_value = v;
		}
		return _value;
	}
	MidiPressure* clone() {
		MidiPressure* newm = MidiPressure::make(Channel(),Pitch(),Value());
		return newm;
	};
private:
	MidiPressure(int ch, int p, int v) : ChanMsg(ch) {
		_pitch = p;
		_value = v;
	};
	int _pitch;
	int _value;
};

class MidiProgramChange : public ChanMsg {
public:
	static MidiProgramChange* make(int ch, int v) {
		MidiProgramChange* m = new MidiProgramChange(ch,v);
		DEBUGPRINT2(("MidiProgramChange::make m=%d",m));
		return m;
	};
	std::string DebugString() {
		return NosuchSnprintf("ProgramChange ch=%d v=%d",_chan,_value);
	}
	PmMessage PortMidiMessage() {
		// Both channel and value going out are 0-based
		return Pm_Message(0xc0 | (_chan-1), _value-1, 0);
	}
	int MidiType() { return MIDI_PROGRAM; }
	int Value(int v = NO_VALUE) {
		if ( v > 0 ) {
			NosuchAssert(v<=128);  // program change value is 1-based
			_value = v;
		}
		return _value;
	}
	MidiProgramChange* clone() {
		MidiProgramChange* newm = MidiProgramChange::make(Channel(),Value());
		return newm;
	};
private:
	MidiProgramChange(int ch, int v) : ChanMsg(ch) {
		NosuchAssert(v>0);  // program change value is 1-based
		_value = v;
	};
	int _value;   // 1-based
};

class MidiPitchBend : public ChanMsg {
public:
	static MidiPitchBend* make(int ch, int v) {
		// The v coming in is expected to be 0 to 16383, inclusive
		MidiPitchBend* m = new MidiPitchBend(ch,v);
		return m;
	};
	std::string DebugString() {
		return NosuchSnprintf("PitchBend ch=%d v=%d",_chan,_value);
	}
	PmMessage PortMidiMessage() {

// The two bytes of the pitch bend message form a 14 bit number, 0 to 16383.
// The value 8192 (sent, LSB first, as 0x00 0x40), is centered, or "no pitch bend."
// The value 0 (0x00 0x00) means, "bend as low as possible,"
// and, similarly, 16383 (0x7F 0x7F) is to "bend as high as possible."

		NosuchAssert(_value >= 0 && _value <= 16383);
		return Pm_Message(0xe0 | (_chan-1), _value & 0x7f, (_value>>7) & 0x7f);
	}
	int MidiType() { return MIDI_PITCHBEND; }
	int Value(int v = NO_VALUE) {
		if ( v >= 0 ) {
			NosuchAssert(v >= 0 && v <= 16383);
			_value = v;
		}
		return _value;
	}
	MidiPitchBend* clone() {
		MidiPitchBend* newm = MidiPitchBend::make(Channel(),Value());
		return newm;
	};
private:
	MidiPitchBend(int ch, int v) : ChanMsg(ch) {
		NosuchAssert(v >= 0 && v <= 16383);
		Value(v);
	};
	int _value;   // from 0 to 16383
};

class MidiPhraseUnit {
public:
	MidiPhraseUnit(MidiMsg* m, click_t c) {
		click = c;
		msg = m;
		next = NULL;
	}
	virtual ~MidiPhraseUnit() {
		delete msg;
	}
	click_t click;  // relative to start of phrase
	MidiMsg* msg;
	MidiPhraseUnit* next;
};

class MidiPhrase {
public:
	MidiPhrase() {
		first = NULL;
		last = NULL;
	}
	virtual ~MidiPhrase() {
		MidiPhraseUnit* nextpu;
		for ( MidiPhraseUnit* pu=first; pu!=NULL; pu=nextpu ) {
			nextpu = pu->next;
			delete pu;
		}
		first = NULL;
		last = NULL;
	}
	void insert(MidiMsg* msg, click_t click);  // takes ownership

#ifdef TOO_EXPENSIVE
	std::string DebugString() {
		std::string s = NosuchSnprintf("MidiPhrase(");
		std::string sep = "";
		for ( MidiPhraseUnit* pu=first; pu!=NULL; pu=pu->next ) {
			s += NosuchSnprintf("%s%st%d",sep.c_str(),pu->msg->DebugString().c_str(),pu->click);
			sep = ",";
		}
		s += ")";
		return s;
	}
	std::string SummaryString() {
		int tot[17] = {0};
		for ( MidiPhraseUnit* pu=first; pu!=NULL; pu=pu->next ) {
			int ch = pu->msg->Channel();
			if ( ch == 0 ) {
				DEBUGPRINT(("Hey!  Channel() returned 0!?"));
			} else if ( ch < 0 ) {
				tot[0]++;
			} else {
				tot[ch]++;
			}
		}
		std::string s = NosuchSnprintf("MidiPhrase(");
		std::string sep = "";
		for ( int ch=1; ch<17; ch++ ) {
			if ( tot[ch] > 0 ) {
				s += NosuchSnprintf("%s%d in ch#%d",sep.c_str(),tot[ch],ch);
				sep = ", ";
			}
		}
		if ( tot[0] > 0 ) {
			s += NosuchSnprintf("%s%d non-channel",sep.c_str(),tot[0]);
		}
		s += ")";
		return s;
	}
#endif

	MidiPhraseUnit* first;
	MidiPhraseUnit* last;
	// eventually put phrase length here
};

MidiPhrase* newMidiPhraseFromFile(std::string filename);

#endif