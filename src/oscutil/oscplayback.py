import re
from nosuch.oscutil import *

if len(sys.argv) < 3:
	print "Usage: oscplayback.py {port@host} {file}"
	sys.exit(1)

if __name__ == '__main__':

	oscmsg = []

	porthost = sys.argv[1]
	print("porthost=",porthost)
	port = re.compile(".*@").search(porthost).group()[:-1]
	host = re.compile("@.*").search(porthost).group()[1:]
	
	filename = sys.argv[2]
	
	f = open(filename)
	r = OscRecipient(host,port)
	time0 = time.time();
	while 1:
		line = f.readline()
		if not line:
			break
		if line[0] != '[':
			continue
		sys.stdout.write("SENDING %s"%line)
		oscbundle = eval(line)
		tm = oscbundle[0]
		while (time.time()-time0) < tm:
			time.sleep(0.001)
		bundle = createBundle()
		n = len(oscbundle)
		for i in range(1,n):
			m = oscbundle[i]
			sys.stdout.write("M = "+str(m))
			appendToBundle(bundle,m[0],m[2:])
		r.sendbundle(bundle)

	f.close()
