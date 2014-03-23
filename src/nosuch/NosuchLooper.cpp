#include "NosuchUtil.h"
#include "mmtt_sharedmem.h"
// #include "Params.h"
// #include "Region.h"
// #include "Channel.h"
// #include "Palette.h"
// #include "Sprite.h"
#include "NosuchLooper.h"
// #include "Cursor.h"
// #include "Sound.h"
#include "NosuchScheduler.h"
// #include "Resolume.h"
// #include "Event.h"

NosuchLooper::NosuchLooper() {
	DEBUGPRINT2(("NosuchLooper constructor"));
	_last_click = 0;
	// _vizlet = b;
	NosuchLockInit(&_looper_mutex,"looper");
}

NosuchLooper::~NosuchLooper() {
	DEBUGPRINT2(("NosuchLooper destructor"));
}

std::string
NosuchLooper::DebugString() {
	Lock();
	std::string s;
	std::vector<NosuchLoop*>::iterator it = _loops.begin();
	int n = 0;
	for(; it != _loops.end(); it++) {
		NosuchLoop *lp = *it;
		NosuchAssert(lp);
		s += NosuchSnprintf("Loop %d\n%s",n++,lp->DebugString("   ").c_str());
	}
	Unlock();
	return s;
}

void
NosuchLooper::AddLoop(NosuchLoop* loop) {
	Lock();
	_loops.push_back(loop);
	Unlock();
}

void
NosuchLooper::AdvanceClickTo(click_t click, NosuchScheduler* sched) {
	Lock();
	int nclicks = click - _last_click;
	static int last_printed_click = 0;
	if ( click >= (last_printed_click+10*sched->ClicksPerSecond) ) {
		DEBUGPRINT1(("NosuchLooper::AdvanceClickTo click=%d now=%d",click,Pt_Time()));
		last_printed_click = click;
	}
	while ( nclicks-- > 0 ) {
		std::vector<NosuchLoop*>::iterator it = _loops.begin();
		for(; it != _loops.end(); it++) {
			NosuchLoop* lp = *it;
			NosuchAssert(lp);
			lp->AdvanceClickBy1();
		}
	}
	_last_click = click;
	Unlock();
}

static bool
loopevent_compare (SchedEvent* first, SchedEvent* second)
{
	if ( first->click < second->click )
		return true;
	else
		return false;
}

void
NosuchLoop::SendMidiLoopOutput(MidiMsg* mm) {
#if 0
	_vizlet->SendMidiMsg();
	// _vizlet->scheduler()->SendMidiMsg(mm,id());
	// noticeMidiOutput(mm,XXX);
#endif
}

std::string
NosuchLoop::DebugString(std::string indent) {
	Lock();
	SchedEventIterator it = _events.begin();
	int n = 0;
	std::string s = NosuchSnprintf("Loop id=%d {\n",_id);
	for(; it != _events.end(); it++) {
		SchedEvent* ep = *it;
		NosuchAssert(ep);
		s += (indent+NosuchSnprintf("   Event %d = %s\n",n++,ep->DebugString().c_str()));
	}
	s += "   }";
	Unlock();
	return s;
}

int
NosuchLoop::AddLoopEvent(SchedEvent* e) {
	int nn;
	Lock();
	_events.push_back(e);
	_events.sort(loopevent_compare);
	nn = NumNotes();
	Unlock();
	return nn;
}

bool
isMatchingOff(SchedEvent* ev, MidiNoteOn* note) {
	MidiMsg* m = ev->u.midimsg;
	if ( m->MidiType() == MIDI_NOTE_OFF
		&& m->Channel() == note->Channel()
		&& m->Pitch() == note->Pitch() ) {
		return true;
	}
	return false;
}

void NosuchLoop::AdvanceClickBy1() {
	Lock();
	_click++;
	int clicklength = QuarterNoteClicks * _looplength;
	if ( _click > clicklength ) {
		_click = 0;
	}

	SchedEventIterator it = _events.begin();
	for(; it != _events.end(); ) {

		SchedEvent* ev = *it;
		NosuchAssert(ev);
		if ( ev->click != _click ) {
			it++;
			continue;
		}

		if ( ev->eventtype() == SchedEvent::CURSORMOTION ) {
			it++;
			DEBUGPRINT(("LOOP CURSORMOTION!  event=%s",ev->DebugString().c_str()));
			continue;
		}

		MidiMsg* m = ev->u.midimsg;
		bool incrementit = true;

		NosuchAssert(m);
		switch ( m->MidiType() ) {
		case MIDI_NOTE_ON: {

			// NOTE: we could apply the decay *after* we play the note,
			// IF we wanted the first iteration of a loop to have
			// the same volume as the original.

			MidiNoteOn* noteon = (MidiNoteOn*)m;
			if ( noteon->Velocity() < MIN_LOOP_VELOCITY ) {

				DEBUGPRINT1(("Note Velocity is < 5, should be removing"));
				it = _events.erase(it);
				incrementit = false;

				// the noteon pointer is still valid, we
				// need to delete it after trying to find the
				// matching noteoff

				SchedEventIterator offit = findNoteOff(noteon,it);
				delete noteon;

				if ( offit != _events.end() ) {
					SchedEvent* evoff = *offit;
					bool updateit = false;
					if ( offit == it ) {
						// We've found the noteoff, and it's the
						// current valule of 'it', so we need to update it.
						updateit = true;
					}
					MidiNoteOff* noteoff = (MidiNoteOff*)(evoff->u.midimsg);
					NosuchAssert(noteoff);
					delete noteoff;
					offit = _events.erase(offit);
					if ( updateit ) {
						it = offit;
					}

					DEBUGPRINT1(("Another NEW DELETE of evoff"));
					delete evoff;
				}
				DEBUGPRINT1(("After removing from lp=%d, nnotes=%d",id(),NumNotes()));

				DEBUGPRINT1(("Another NEW DELETE of ev"));
				delete ev;

				int nnotes;
				int ncontrollers;
				NumEvents(nnotes,ncontrollers);
				if ( nnotes == 0 ) {
					DEBUGPRINT(("No more notes in loop, CLEARING IT!"));
					ClearNoLock();
					// XXX - should be forcing controller value to 0
					goto getout;
				}

			} else {
				SendMidiLoopOutput(noteon);
				// Fade the velocity
				double fade = _loopfade;
				DEBUGPRINT1(("Fading velocity, vel=%d fade=%.4f",(int)(noteon->Velocity()),fade));
				noteon->Velocity((int)(noteon->Velocity()*fade));
			}
			break;
			}
		case MIDI_NOTE_OFF: {
			MidiNoteOff* noteoff = (MidiNoteOff*)m;
			SendMidiLoopOutput(noteoff);
			break;
			}
		case MIDI_CONTROL: {
			MidiController* ctrl = (MidiController*)m;
			SendMidiLoopOutput(ctrl);
			break;
			}
		default:
			DEBUGPRINT(("AdvanceClickBy1 can't handle miditype=%d",m->MidiType()));
			break;
		}
		if ( incrementit ) {
			it++;
		}
	}
getout:
	Unlock();
}

SchedEventIterator
NosuchLoop::findNoteOff(MidiNoteOn* noteon, SchedEventIterator& it) {
	// Look for the matching noteoff, IF it exists in the loop.
	SchedEventIterator offit = it;
	MidiNoteOff* noteoff = NULL;
	for( ; offit != _events.end(); offit++ ) {
		if ( isMatchingOff(*offit,noteon) ) {
			return offit;
		}
	}
	// Didn't find it yet, scan from beginning
	offit = _events.begin();
	for( ; offit != _events.end(); offit++ ) {
		if ( isMatchingOff(*offit,noteon) ) {
			return offit;
		}
	}
	return _events.end();
}

SchedEventIterator
NosuchLoop::oldestNoteOn() {
	SchedEventIterator it = _events.begin();
	click_t oldest = INT_MAX;
	SchedEventIterator oldest_it;
	for( ; it != _events.end(); it++ ) {
		SchedEvent* ev = *it;
		NosuchAssert(ev);
		if ( ev->eventtype() != SchedEvent::MIDIMSG ) {
			continue;
		}
		MidiMsg* mm = ev->u.midimsg;
		NosuchAssert(mm);
		if ( mm->MidiType() == MIDI_NOTE_ON ) {
			if ( ev->created() < oldest ) {
				oldest_it = it;
				oldest = ev->created();
			}
		}
	}
	if ( oldest < INT_MAX ) {
		return oldest_it;
	} else {
		return _events.end();
	}
}

void
NosuchLoop::removeNote(SchedEventIterator it) {
	// We assume we've been given an iterator that points to a noteon
	SchedEvent* ev = *it;
	NosuchAssert(ev);
	if ( ev->eventtype() != SchedEvent::MIDIMSG ) {
		throw NosuchException("removeNote got event that's not MIDI");
	}
	if ( ev->u.midimsg->MidiType() != MIDI_NOTE_ON ) {
		throw NosuchException("removeNote didn't get a noteon");
	}
	MidiNoteOn* noteon = (MidiNoteOn*)ev->u.midimsg;
	NosuchAssert(noteon);

	it = _events.erase(it);
	SchedEventIterator offit = findNoteOff(noteon,it);
	delete noteon;
}

void
NosuchLoop::removeOldestNoteOn() {
	Lock();
	SchedEventIterator it = oldestNoteOn();
	if ( it == _events.end() ) {
		DEBUGPRINT(("Hmmm, removeOldestNoteOn didn't find an oldest note!?"));
		Unlock();
		return;
	}
	MidiNoteOn* noteon = (MidiNoteOn*)((*it)->u.midimsg);
	NosuchAssert(noteon);
	it = _events.erase(it);
	SchedEventIterator offit = findNoteOff(noteon,it);
	delete noteon;
	if ( offit == _events.end() ) {
		DEBUGPRINT(("Hmmm, removeOldestNoteOn didn't find noteoff for oldest note!?"));
	} else {
		SchedEvent* ev = *offit;
		MidiNoteOff* noteoff = (MidiNoteOff*)(ev->u.midimsg);
		NosuchAssert(noteoff);
		_events.erase(offit);
		delete noteoff;

		DEBUGPRINT1(("NEW DELETE CODE IN removeOldestNoteOn"));
		delete ev;
	}
	Unlock();
}

int
NosuchLoop::NumNotes() {
	int nnotes;
	int ncontrollers;
	NumEvents(nnotes,ncontrollers);
	return nnotes;
}

void
NosuchLoop::NumEvents(int& nnotes, int& ncontrollers) {
	SchedEventIterator it = _events.begin();
	nnotes = 0;
	ncontrollers = 0;
	for(; it != _events.end(); it++ ) {
		SchedEvent* ev = *it;
		NosuchAssert(ev);
		if ( ev->eventtype() != SchedEvent::MIDIMSG ) {
			continue;
		}
		MidiMsg* m = ev->u.midimsg;
		NosuchAssert(m);
		switch (m->MidiType()) {
		case MIDI_NOTE_ON:
			nnotes++;
			break;
		case MIDI_CONTROL:
			ncontrollers++;
			break;
		}
	}
	return;
}

void
NosuchLoop::Clear() {
	Lock();
	ClearNoLock();
	Unlock();
}

void
NosuchLoop::ClearNoLock() {
	while ( true ) {
		SchedEventIterator it = _events.begin();
		if ( it == _events.end() ) {
			break;
		}
		SchedEvent* ev = *it;
		NosuchAssert(ev);
		switch (ev->eventtype()) {
		case SchedEvent::MIDIMSG: {
			MidiMsg* m = ev->u.midimsg;
			if ( m->MidiType() == MIDI_NOTE_OFF ) {
				MidiNoteOff* noteoff = (MidiNoteOff*)m;
				SendMidiLoopOutput(noteoff);
			}
			NosuchAssert(m);
			delete m;
			break;
			}
		case SchedEvent::CURSORMOTION: {
			NosuchCursorMotion* cm = ev->u.cursormotion;
			DEBUGPRINT(("Clearing Cursormotion event!"));
			delete cm;
			break;
			}
		}
		_events.erase(it);
		delete ev;
	}
}