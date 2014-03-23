import time
from nosuch.oscutil import *

r = OscRecipient("127.0.0.1",44444)

def tobeamer(addr,msg):
	b = createBinaryMsg(addr,msg)
	r.osc_socket.sendto(b, (r.osc_host, r.osc_port))
	print "Sent ",b

tobeamer("/run",[])
tobeamer("/clear",[])
tobeamer("/square",["x=0.5","y=0.5"])
# time.sleep(5)
# tobeamer("/clear",[])
# time.sleep(5)
# tobeamer("/stop",[])
