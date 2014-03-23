from nosuch.midiutil import *
from nosuch.midifile import *
from nosuch.oscutil import *
from nosuch.midiosc import *
from traceback import format_exc
from time import sleep
import sys

time0 = time.time()

def mycallback(ev,outfile):
	global time0
	tm = time.time()-time0
	line = "[%.6f"%tm
	for m in ev.oscmsg:
		line = line + "," + str(m)
	line = line + "]\n"
	outfile.write(line)

if __name__ == '__main__':

	if len(sys.argv) < 3:
		print "Usage: oscrecord {port@addr} {outputfile}"
		sys.exit(1)

	input_name = sys.argv[1]
	output_name = sys.argv[2]

	port = re.compile(".*@").search(input_name).group()[:-1]
	host = re.compile("@.*").search(input_name).group()[1:]

	print "host=",host," port=",port," outputfile=",output_name
	outfile = open(output_name,"w")
	oscmon = OscMonitor(host,port)
	oscmon.setcallback(mycallback,outfile)

	sleep(3600)  # an hour
