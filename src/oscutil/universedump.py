import re
import time
import random
from nosuch.oscutil import *
from traceback import format_exc
from time import sleep
import sys

global id
id = 0

def send(oscaddr,oscmsg):
	# print "Sending oscaddr=",oscaddr," oscmsg=",oscmsg
	universe.sendosc(oscaddr,oscmsg)
	
if __name__ == '__main__':

	port=44444
	host="localhost"
	global universe
	universe = OscRecipient(host,port,proto="tcp")

	input_name = "3333@127.0.0.1"
	port = re.compile(".*@").search(input_name).group()[:-1]
	host = re.compile("@.*").search(input_name).group()[1:]

	send("/dump",[])

sys.exit(0)
