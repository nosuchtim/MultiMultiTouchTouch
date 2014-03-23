import re
from nosuch.oscutil import *

if len(sys.argv) < 1:
	print "Usage: oscsendtcptest.py {port@host}"
	sys.exit(1)

if __name__ == '__main__':

	porthost = sys.argv[1]
	print("porthost=",porthost)
	port = re.compile(".*@").search(porthost).group()[:-1]
	host = re.compile("@.*").search(porthost).group()[1:]
	
	r = OscRecipient(host,port,proto="tcp")

	oscmsg = [1,2,3]
	r.sendosc("/test",oscmsg)

	oscmsg = [1.0,2.0,3.0]
	r.sendosc("/test",oscmsg)

	oscmsg = ["hello","world"]
	r.sendosc("/test",oscmsg)

	SLIP_END = chr(192)
	SLIP_ESC = chr(219)
	SLIP_ESC2 = chr(221)

	oscmsg = ["Hi"+SLIP_ESC+SLIP_ESC+"By"]
	r.sendosc("/test",oscmsg)

	oscmsg = ["Hi"+SLIP_END+"By"]
	r.sendosc("/test",oscmsg)
