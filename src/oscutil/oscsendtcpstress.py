import re
from nosuch.oscutil import *

if len(sys.argv) < 2:
	print "Usage: oscsendtcptest.py {port@host} {test-length}"
	sys.exit(1)

if __name__ == '__main__':

	porthost = sys.argv[1]
	nn = int(sys.argv[2])
	print("porthost=",porthost)
	port = re.compile(".*@").search(porthost).group()[:-1]
	host = re.compile("@.*").search(porthost).group()[1:]
	
	r = OscRecipient(host,port,proto="tcp")

	for n in range(nn):
		import random
		s = ""
		sleng = random.randint(0,7000)
		print "String len = ",sleng
		# fmt = "s = "
		for i in range(sleng):
			rc = random.randint(1,255)
			s += chr(rc)
			# fmt += "(\\x%02x)" % rc
			# s += chr(random.randint(60,70))
		# print fmt
		oscmsg = [random.randint(0,sys.maxint),random.random(),s]
		r.sendosc("/test",oscmsg)
