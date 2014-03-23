import re

from nosuch.oscutil import *
from traceback import format_exc
from time import sleep
import sys

def mycallback(ev,d):
	o = ev.oscmsg
	x = 0.0
	y = 0.0
	# print "o=",o
	for m in o:
		addr = m[0]
		# print "   addr = ",addr," m2=",m[2]
		if addr == "/tuio/2Dcur" and m[2] == "set":
			print "2Dcur set id=",m[3]," xy=",m[4],m[5]
			x = m[4]
			y = m[5]
		if addr == "/tuio/25Dcur" and m[2] == "set":
			print "25Dcur set id=",m[3]," xy=",m[4],m[5],m[6]
			x = m[4]
			y = m[5]
		if addr == "/tuio/_ssif" and m[2] == "set":
			print "IGESTURE xy=",x,y," id=",m[4]," force=",m[5]

if __name__ == '__main__':

	if len(sys.argv) < 2:
		print "Usage: osclisten {port@addr}"
		sys.exit(1)

	input_name = sys.argv[1]
	port = re.compile(".*@").search(input_name).group()[:-1]
	host = re.compile("@.*").search(input_name).group()[1:]

	oscmon = OscMonitor(host,port)
	oscmon.setcallback(mycallback,"")

	sleep(100000)
