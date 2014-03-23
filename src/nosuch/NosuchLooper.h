#ifndef _NOSUCH_LOOP_EVENT
#define _NOSUCH_LOOP_EVENT

#include <list>
#include "NosuchScheduler.h"

#define DEFAULT_LOOPFADE 0.7f
#define MIN_LOOP_VELOCITY 5

class SchedEvent;

typedef std::list<SchedEvent*> SchedEventList;

class Vizlet;
class Region;

class NosuchLoop {
public:
	NosuchLoop(Vizlet* mf, int id, int looplength, double loopfade) {
		_vizlet = mf;
		_click = 0;
		_looplength = looplength;
		_loopfade = loopfade;
		_id = id;
		NosuchLockInit(&_loop_mutex,"loop");
	};
	~NosuchLoop() {
		DEBUGPRINT1(("NosuchLoop DESTRUCTOR!"));
	}
	void Clear();
	void AdvanceClickBy1();
	int AddLoopEvent(SchedEvent* e);
	std::string DebugString(std::string indent = "");
	void SendMidiLoopOutput(MidiMsg* mm);
	int id() { return _id; }
	click_t click() { return _click; }
	int NumNotes();
	void NumEvents(int& nnotes, int& ncontrollers);
	void removeOldestNoteOn();

private:
	SchedEventList _events;
	click_t _click; // relative within loop
	int _id;
	Vizlet* _vizlet;
	int _looplength;
	double _loopfade;

	pthread_mutex_t _loop_mutex;
	void Lock() { NosuchLock(&_loop_mutex,"loop"); }
	void Unlock() { NosuchUnlock(&_loop_mutex,"loop"); }

	SchedEventIterator findNoteOff(MidiNoteOn* noteon, SchedEventIterator& it);
	SchedEventIterator oldestNoteOn();
	void removeNote(SchedEventIterator it);
	void ClearNoLock();
};

class NosuchLooper : public NosuchClickListener {
public:
	NosuchLooper(/*Vizlet* b*/);
	~NosuchLooper();
	void AdvanceClickTo(click_t click, NosuchScheduler* s);
	std::string DebugString();
	void AddLoop(NosuchLoop* loop);

private:
	std::vector<NosuchLoop*> _loops;
	int _last_click;
	Vizlet* _vizlet;
	pthread_mutex_t _looper_mutex;

	void Lock() { NosuchLock(&_looper_mutex,"looper"); }
	void Unlock() { NosuchUnlock(&_looper_mutex,"looper"); }

};

#endif