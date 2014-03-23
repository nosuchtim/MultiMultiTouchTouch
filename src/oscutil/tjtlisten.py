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
		if addr == "/tuio/25Dblb" and m[2] == "set":
			print "25Dblb set id=",m[3]," xyz=",m[4],m[5],m[6]
			x = m[4]
			y = m[5]
			z = m[6]

if __name__ == '__main__':

	port = "3333"
	host = "127.0.0.1"

	oscmon = OscMonitor(host,port)
	oscmon.setcallback(mycallback,"")

	sleep(100000)
