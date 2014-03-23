import re

from nosuch.oscutil import *
from traceback import format_exc
from time import sleep
import sys

def mycallback(ev,d):
	o = ev.oscmsg
	x = 0.0
	y = 0.0
	print "o=",o
	# for m in o:
	# 	addr = m[0]
	# 	print "   addr = ",addr," m2=",m[2]

if __name__ == '__main__':

	if len(sys.argv) < 2:
		print "Usage: osclisten {port@addr}"
		sys.exit(1)

	input_name = sys.argv[1]
	port = re.compile(".*@").search(input_name).group()[:-1]
	host = re.compile("@.*").search(input_name).group()[1:]

	oscmon = OscMonitor(host,port)
	oscmon.callback(mycallback,"")

	sleep(100000)
