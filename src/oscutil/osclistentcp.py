from nosuch.midiutil import *
from nosuch.midifile import *
from nosuch.oscutil import *
from nosuch.midiosc import *
from traceback import format_exc
from time import sleep
import sys

def mycallback(ev,d):
	s = str(ev)
	if len(s) > 200:
		print "Received OSC: ",s[0:200]," ...(truncated)"
	else:
		print "Received OSC: ",s

if __name__ == '__main__':

	if len(sys.argv) < 2:
		print "Usage: osclisten {port@addr}"
		sys.exit(1)

	input_name = sys.argv[1]
	port = re.compile(".*@").search(input_name).group()[:-1]
	host = re.compile("@.*").search(input_name).group()[1:]

	oscmon = OscMonitor(host,port,proto="tcp")
	oscmon.callback(mycallback,"")

	sleep(10000)
