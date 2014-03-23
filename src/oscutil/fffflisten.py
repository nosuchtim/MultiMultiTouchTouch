from nosuch.midiutil import *
from nosuch.midifile import *
from nosuch.oscutil import *
from nosuch.midiosc import *
from traceback import format_exc
from time import sleep
import sys

showalive = False
showfseq = False
time0 = time.time()

def mycallback(ev,d):
	for m in ev.oscmsg:
		if showalive==False and len(m) >= 3 and ( m[2] == "alive" ):
			pass
		elif showfseq==False and len(m) >= 3 and ( m[2] == "fseq" ):
			pass
		else:
			print m

if __name__ == '__main__':

	if len(sys.argv) < 2:
		print "Usage: osclisten [-a] {port@addr}"
		sys.exit(1)

	if sys.argv[1] == "-a":
		showalive = True
		showfseq = True
		input_name = sys.argv[2]
	else:
		input_name = sys.argv[1]

	port = re.compile(".*@").search(input_name).group()[:-1]
	host = re.compile("@.*").search(input_name).group()[1:]

	oscmon = OscMonitor(host,port)
	oscmon.setcallback(mycallback,"")

	sleep(10000)
