from nosuch.midiutil import *
from nosuch.midifile import *
from nosuch.midipypm import *
from nosuch.oscutil import *
from nosuch.midiosc import *
from traceback import format_exc
from time import sleep
import sys

showalive = False
showfseq = False
time0 = time.time()

Scales = {
		"Ionian": [0,2,4,5,7,9,11],
		"Dorian": [0,2,3,5,7,9,10],
		"Phrygian": [0,1,3,5,7,8,10],
		"Lydian": [0,2,4,6,7,9,11],
		"Mixolydian": [0,2,4,5,7,9,10],
		"Aeolian": [0,2,3,5,7,8,10],
		"Locrian": [0,1,3,5,6,8,10],
		"Newage": [0,3,5,7,10],
		"Fifths": [0,7],
		"Octaves": [0],
		"Harminor": [0,2,3,5,7,8,11],
		"Melminor": [0,2,3,5,7,9,11],
		"Chromatic": [0,1,2,3,4,5,6,7,8,9,10,11]
		}

def make_scalenotes(scalename):
	# Construct an array of 128 elements with the mapped
	# pitch of each note to a given scale of notes
	scale = Scales[scalename]

	# Adjust the incoming scale to the current key
	realscale = []
	keyindex = 0
	for i in range(len(scale)):
		realscale.append((scale[i] + keyindex) % 12)

	global Scalenotes
	Scalenotes = []
	# Make an array mapping each pitch to the closest scale note.
	# This code is brute-force, it starts at the pitch and
	# goes incrementally up/down from it until it hits a pitch
	# that's on the scale.
	for pitch in range(128):
		Scalenotes.append(pitch)
		inc = 1
		sign = 1
		cnt = 0
		p = pitch
		# the cnt is just-in-case, to prevent an infinite loop
		while cnt < 100:
			if p >=0 and p <= 127 and ((p%12) in realscale):
				break
			cnt += 1
			p += (sign * inc)
			inc += 1
			sign = -sign
		if cnt >= 100:
			print "Something's amiss in set_scale!?"
			p = pitch
		Scalenotes[pitch] = p

Sids = {}
Fseq = 0

def velocityof(z):
	v = 10 + z * 1000
	if v > 127:
		v = 127
	return v

def velocityofx(x):
	v = 10 + x * 100
	if v > 127:
		v = 127
	print "velocityofx x=",x," v= ",v
	return v

def pitchofh(h):
	h = h / 500.0
	p = 50 + h * 60
	print "pitchofh p=",p
	p = Scalenotes[int(p)]
	print "scaled p=",p
	return p

def pitchof(x):
	p = 60 + x * 40
	print "p=",p
	p = Scalenotes[int(p)]
	print "scaled p=",p
	return p

def quantofh(h):
	print "quantofh h = ",h
	if h < 200:
		return 0.5
	if h < 300:
		return 0.25
	if h < 400:
		return 0.125
	return 0

def quantof(y):
	if y < 0.25:
		return 0.25
	if y < 0.5:
		return 0.125
	if y < 0.75:
		return 0.0625
	return 0

def chanof(x):
	return 4
	if x < 0.25:
		return 1
	if x < 0.5:
		return 2
	if x < 0.75:
		return 3
	return 4

def nextquant(tm,q):
	# We assume values are in terms of seconds
	if q <= 0:
		return tm
	q1000 = int(q * 1000)
	tm1000 = int(tm * 1000)
	tmq = tm1000 % q1000
	dq = (tmq/1000.0)
	nextq = tm + (q-dq)
	# print "nextquant tm=%f q=%f tmq=%d dq=%f nextq=%f" % (tm,q,tmq,dq,nextq)
	return nextq

def cursorDown(state):
	# print "DOWN sid=",state.sid," x=",state.x," y=",state.y," z=",state.z
	p = pitchof(state.x)
	p = pitchofh(state.h)
	ch = chanof(state.x)
	v = velocityof(state.z)
	v = velocityofx(state.x)
	d = 0.0625 * Midi.clocks_per_second
	d = 1.0 * Midi.clocks_per_second
	m = SequencedNote(pitch=p,duration=d,channel=ch,velocity=v)
	print "    M = ",m
	tm = time.time()
	q = quantof(state.y)
	q = quantofh(state.h)
	tm = nextquant(tm,q)
	MidiOut.schedule(m,tm)

def cursorDrag(state):
	# print "DRAG sid=",state.sid," x=",state.x," y=",state.y," z=",state.z
	cursorDown(state)

def cursorUp(state):
	# print "Up sid=",state.sid," x=",state.x," y=",state.y," z=",state.z
	pass

class CursorState:
	def __init__(self,fseq,sid):
		self.fseq = fseq
		self.sid = sid
		self.isdown = False

def mycallback(ev,d):
	global Fseq, Sids
	for m in ev.oscmsg:
		lenm = len(m)
		if m[0] != "/tuio/25Dblb":
			pass
		if lenm >= 3 and m[2] == "alive":
			for i in range(3,len(m)):
				sid = m[i]
				if sid in Sids:
					Sids[sid].fseq = Fseq
				else:
					Sids[sid] = CursorState(Fseq,sid)
			toremove = []
			for k in Sids:
				if Sids[k].fseq != Fseq:
					toremove.append(k)
			for k in toremove:
				cursorUp(Sids[k])
				del Sids[k]
		elif lenm >= 3 and m[2] == "fseq":
			Fseq = m[3]
		elif lenm >= 3 and m[2] == "set":
			# print m
			sid = m[3]
			x = m[4]
			y = m[5]
			z = m[6]
			w = m[8]
			h = m[9]
			print "w=",w," h=",h
			if sid in Sids:
				state = Sids[sid]
				state.x = x
				state.y = y
				state.z = z
				state.w = w
				state.h = h
				# print "updated sid = ",state.x," ",state.y," ",state.z
				if state.isdown:
					cursorDrag(state)
				else:
					cursorDown(state)
					state.isdown = True
			else:
				print "sid ",sid," isn't in Sids!?"

if __name__ == '__main__':

	input_name = "3333@127.0.0.1"

	port = re.compile(".*@").search(input_name).group()[:-1]
	host = re.compile("@.*").search(input_name).group()[1:]

	Midi.startup()
	mididev = MidiPypmHardware()
	outs = mididev.output_devices()
	print "midi outs = ",outs
	outname = "Microsoft GS Wavetable Synth"
	outname = "01. Internal MIDI"
	try:
		global MidiOut
		MidiOut = mididev.get_output(outname)
		MidiOut.open()
		print "opened midiout = ",outname
	except:
		print "Error openging MIDI output - ",outname
		sys.exit(1)

	# tm = time.time()
	# for p in range(60,70):
	# 	m = SequencedNote(pitch=p,duration=1,channel=1,velocity=100)
	# 	MidiOut.schedule(m,time=tm)
	# 	tm += 0.1

	make_scalenotes("Newage")
	global Scalenotes
	print "Scalenotes = ",Scalenotes

	oscmon = OscMonitor(host,port)
	oscmon.setcallback(mycallback,"")

	sleep(10000)
