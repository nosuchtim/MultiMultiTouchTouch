#ifndef _NOSUCHSCHEDULER_H
#define _NOSUCHSCHEDULER_H

typedef int click_t;

#include "NosuchUtil.h"
#include "NosuchMidi.h"
#include "porttime.h"
#include "portmidi.h"
#include "pmutil.h"
#include "NosuchGraphics.h"
#include <pthread.h>
#include <list>
#include <map>
#include <algorithm>

#define IN_QUEUE_SIZE 1024
#define OUT_QUEUE_SIZE 1024

// Session IDs (TUIO cursor sessions) and Loop IDs need to be distinct,
// since these IDs are attached to scheduled events so we can know
// who was responsible for creating the events.

// Loop IDs are session IDs that are >= LOOPID_BASE.
// Each region has one loop.
#define LOOPID_BASE 1000000
#define LOOPID_OF_REGION(id) (LOOPID_BASE+id)

extern int SchedulerCount;
extern click_t GlobalClick;
extern int GlobalPitchOffset;
extern bool NoMultiNotes;
extern bool LoopCursors;

class NosuchScheduler;
class MidiMsg;
class MidiPhrase;

class NosuchClickListener {
public:
	virtual void AdvanceClickTo(int current_click, NosuchScheduler* sched) = 0;
};

class NosuchMidiListener {
public:
	virtual void processMidiMsg(MidiMsg* mm) = 0;
};

class NosuchMidiInputListener {
public:
	virtual void processMidiInput(MidiMsg* mm) = 0;
};

class NosuchMidiOutputListener {
public:
	virtual void processMidiOutput(MidiMsg* mm) = 0;
};

class NosuchCursorMotion {
public:
	NosuchCursorMotion(int downdragup, NosuchVector pos, float depth) {
		_downdragup = downdragup;
		_pos = pos;
		_depth = depth;
	}
	int _downdragup;
	NosuchVector _pos;
	float _depth;
};

class SchedEvent {
public:
	enum Type {
		MIDIMSG,
		CURSORMOTION,
		MIDIPHRASE
	};
	SchedEvent(MidiMsg* m, click_t c, void* h) {
		_eventtype = SchedEvent::MIDIMSG;
		u.midimsg = m;
		click = c;
		handle = h;
		_created = GlobalClick;
	}
	SchedEvent(MidiPhrase* ph, click_t c, void* h) {
		_eventtype = SchedEvent::MIDIPHRASE;
		u.midiphrase = ph;
		click = c;
		handle = h;
		_created = GlobalClick;
	}
	SchedEvent(NosuchCursorMotion* cm,click_t c, void* h) {
		_eventtype = SchedEvent::CURSORMOTION;
		u.midimsg = NULL;
		u.cursormotion = cm;
		click = c;
		handle = h;
		_created = GlobalClick;
	}
	~SchedEvent();
	std::string DebugString();

	click_t click;	// relative when in loops, absolute elsewhere
	union {
		MidiMsg* midimsg;
		MidiPhrase* midiphrase;
		NosuchCursorMotion* cursormotion;
	} u;

	int eventtype() { return _eventtype; }
	click_t created() { return _created; }

protected:
	void* handle;	// handle to thing that created it
	friend class NosuchScheduler;

private:

	int _eventtype;
	int _ended;  // if 1, it's a SessionEnd event
	click_t _created;	// absolute
};

typedef std::list<SchedEvent*> SchedEventList;
typedef std::list<SchedEvent*>::iterator SchedEventIterator;

class NosuchScheduler {
public:
	NosuchScheduler() {
		DEBUGPRINT1(("NosuchScheduler CONSTRUCTED!!, count=%d",SchedulerCount++));
		m_running = false;
		clicknow = 0;
		SetMilliNow(0);
#ifdef NOWPLAYING
		_nowplaying_note.clear();
		// _nowplaying_controller.clear();
#endif

		SetClicksPerSecond(192);

		m_clicks_per_clock = 4;
		NosuchLockInit(&_scheduled_mutex,"scheduled");
		NosuchLockInit(&_notesdown_mutex,"notesdown");
		_midioutput_client = NULL;
		_midi_input = NULL;
		_midi_output = NULL;
		_click_client = NULL;
		_periodic_ANO = false;
	}
	~NosuchScheduler() {
		DEBUGPRINT(("NosuchScheduler destructor!"));
		Stop();
	}
	void SetClicksPerSecond(int clkpersec);
	void SetTempoFactor(float f);
	SchedEventList* ScheduleOf(void* handle);
	void SetClickProcessor(NosuchClickListener* client) {
		_click_client = client;
	}
	void SetMidiInputListener(NosuchMidiListener* client) {
		_midiinput_client = client;
	}
	void SetMidiOutputListener(NosuchMidiListener* client) {
		_midioutput_client = client;
	}
	bool StartMidi(std::string midi_input, std::string midi_output);
	void Stop();
	void AdvanceTimeAndDoEvents(PtTimestamp timestamp);
	void Callback(PtTimestamp timestamp);
	std::string DebugString();

	// int NumberScheduled(click_t minclicks, click_t maxclicks, std::string sid);
	bool AnythingPlayingAt(click_t clk, void* handle);
	void IncomingNoteOff(click_t clk, int ch, int pitch, int vel, void* handle);
	void ScheduleMidiMsg(MidiMsg* m, click_t clk, void* handle);
	void ScheduleMidiPhrase(MidiPhrase* m, click_t clk, void* handle);
	void ScheduleClear();
	void SendPmMessage(PmMessage pm, void* handle);
	void SendMidiMsg(MidiMsg* mm, void* handle);
	void SendControllerMsg(MidiMsg* m, void* handle, bool smooth);  // gives ownership of m away
	void SendPitchBendMsg(MidiMsg* m, void* handle, bool smooth);  // gives ownership of m away
	void ANO(int ch = -1);
	void setPeriodicANO(bool b) { _periodic_ANO = b; }

	static int _MilliNow;
	void SetMilliNow(int tm) {
		_MilliNow = tm;
	}
	click_t ClickNow() { return clicknow; }

	int ClicksPerSecond;
	double ClicksPerMillisecond;
	click_t clicknow;
	int millitime0;
	int LastTimeStamp;
	PmStream *midi_input() { return _midi_input; }

	std::list<MidiMsg*>& NotesDown() {
		return _notesdown;
	}

	void ClearNotesDown() {
		LockNotesDown();
		_notesdown.clear();
		UnlockNotesDown();
	}

	void LockNotesDown() { NosuchLock(&_notesdown_mutex,"notesdown"); }
	void UnlockNotesDown() { NosuchUnlock(&_notesdown_mutex,"notesdown"); }

private:

	void _maintainNotesDown(MidiMsg* m);
	std::list<MidiMsg*> _notesdown;

	int TryLockScheduled() { return NosuchTryLock(&_scheduled_mutex,"sched"); }
	void LockScheduled() { NosuchLock(&_scheduled_mutex,"sched"); }
	void UnlockScheduled() { NosuchUnlock(&_scheduled_mutex,"sched"); }

	bool AddEvent(SchedEvent* e, bool lockit=true);  // returns false if not added, i.e. means caller should free it
	void SortEvents(SchedEventList* sl);
	void DoEventAndDelete(SchedEvent* e, void* handle);
	void DoMidiMsgEvent(MidiMsg* m, void* handle);
	bool m_running;
	int m_clicks_per_clock;

	// Per-handle scheduled events
	std::map<void*,SchedEventList*> _scheduled;

#ifdef NOWPLAYING
	// This is a mapping of session id (either TUIO session id or Looping id)
	// to whatever MIDI message (i.e. notes) are currently active (and need a
	// noteoff if we change).
	std::map<int,MidiMsg*> _nowplaying_note;

	// This is a mapping of session id (either TUIO session id or Looping id)
	// to whatever the last MIDI controllers were.  The
	// map inside the map is a mapping of controller numbers
	// to the messages.
	std::map<int,std::map<int,MidiMsg*>> _nowplaying_controller;

	// This is a mapping of session id (either TUIO session id or Looping id)
	// to whatever the last pitchbend was.
	std::map<int,MidiMsg*> _nowplaying_pitchbend;
#endif

	PmStream *_midi_input;
	PmStream *_midi_output;
	NosuchClickListener* _click_client;
	NosuchMidiListener*	_midioutput_client;
	NosuchMidiListener*	_midiinput_client;
	pthread_mutex_t _scheduled_mutex;
	pthread_mutex_t _notesdown_mutex;
	bool _periodic_ANO;
};

#endif
