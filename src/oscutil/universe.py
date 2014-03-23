import re
import time
import random
from nosuch.oscutil import *
from traceback import format_exc
from time import sleep
import sys

global id
id = 0

def mycallback(ev,d):
	o = ev.oscmsg
	x = 0.0
	y = 0.0
	# print "ev=",ev," d=",d," source_ip=",ev.source_ip
	for m in o:
		addr = m[0]
		# print "   addr = ",addr," m2=",m[2]
		if addr == "/tuio/2Dcur" and m[2] == "set":
			print "2Dcur id=",m[3]," xy=%.4f,%.4f"%(m[4],m[5]),ev.source_ip
			x = m[4]
			y = m[5]
			z = 0.2
			x = x * 2.0 - 1.0
			y = (1.0-y) * 2.0 - 1.0
			send("/square",["x=%f"%x,"y=%f"%y,"hue=*COLORLFO1","scalex=%f"%z,"scaley=%f"%z,"alpha=!ALPHAENV","lifetime=3.0"])
		if addr == "/tuio/25Dcur" and m[2] == "set":
			print "25Dcur id=",m[3]," xy=%.4f,%.4f"%(m[4],m[5]),m[6],ev.source_ip
			x = m[4]
			y = m[5]
			z = m[6]
			global id
			id += 1
			x = x * 2.0 - 1.0
			y = (1.0-y) * 2.0 - 1.0
			send("/square",["x=%f"%x,"y=%f"%y,"hue=*COLORLFO2","scalex=%f"%z,"scaley=%f"%z,"alpha=!ALPHAENV","lifetime=3.0"])
		if addr == "/tuio/_ssif" and m[2] == "set":
			print "IGESTURE xy=",x,y," id=",m[4]," force=",m[5]

def send(oscaddr,oscmsg):
	# print "Sending oscaddr=",oscaddr," oscmsg=",oscmsg
	try:
		global universe, universe_host, universe_port
		universe.sendosc(oscaddr,oscmsg)
	except:
		global universe, universe_host, universe_port
		print "SEND EXCEPTION !!! =",format_exc()
		universe = OscRecipient(universe_host,universe_port,proto="tcp")
		initstuff()
		print "TRYING AGAIN!"
		universe.sendosc(oscaddr,oscmsg)
	
def initstuff():
	send("/clear",[])
	send("/run",[])
	send("/lfo",["name=COLORLFO1","center=0.5","amp=1.0","freq=0.002", "tag=mytag"])
	send("/lfo",["name=COLORLFO2","center=0.5","amp=1.0","freq=0.02", "tag=mytag"])
	send("/event",["target=COLORLFO1","type=bang"])
	send("/event",["target=COLORLFO2","type=bang"])
	send("/env",["name=ENV1","start=0.5","end=0.0","dur=3.0"])
	send("/env",["name=ENV1","start=0.5","end=0.0","dur=0.5"])
	send("/env",["name=ALPHAENV","start=0.3","end=0.01","dur=3.0"])
	send("/env",["name=SCALE_ENV1","start=0.5","end=0.1","dur=0.5"])
	# send("/event",["target=LFO3","type=bang"])
	# send("/event",["target=ENV1","type=bang"])
	# send("/square",["x=!LFO3","y=0.0","hue=0.5","scalex=0.2","scaley=0.2","alpha=0.5","lifetime=50.0"])
	sleep(1.0)
	send("/square",["x=*ENV1","y=0.3","hue=0.7","scalex=0.2","scaley=0.2","alpha=!ALPHAENV","lifetime=10.0"])
	send("/event",["target=ENV1","type=bang"])

if __name__ == '__main__':

	global universe, universe_port, universe_host
	universe_port=5555
	universe_host="localhost"
	universe = OscRecipient(universe_host,universe_port,proto="tcp")

	input_name = "6666@127.0.0.1"
	input_name = "6666@192.168.1.102"
	port = re.compile(".*@").search(input_name).group()[:-1]
	host = re.compile("@.*").search(input_name).group()[1:]

	initstuff()

	oscmon = OscMonitor(host,port,proto="udp")
	oscmon.setcallback(mycallback,"")

	sleep(100000)


sys.exit(0)

time.sleep(20.0)
send("/env",["name=ENV1","start=0.9","end=0.1","dur=2.0"])
send("/env",["name=A2","start=0.9","end=0.1","dur=2.0"])
send("/square",["name=SQ1","x=!A2","alpha=!A2"])

time.sleep(14.0)
send("/list",[])


# send("/square",["x=-0.99","y=-0.99","hue=0.0","scalex=1.98","scaley=1.98","handlex=0.0","handley=0.0","alpha=0.2"])

k=0
i=0
# lfoname= "LFO%04d%04d"%(k,i)
# send("/lfo",["name="+lfoname,"center=0.0","amp=1.0","freq=%f"%(0.1*random.random())])
# send("/event",["target="+lfoname,"type=bang"])
envname= "ENV%04d%04d"%(k,i)
send("/env",["name="+envname,"start=1.0","end=0.0","dur=3.0"])
send("/square",["x=0.0","y=0.0","hue=%f"%random.random(),"scalex=0.2","scaley=0.2","handlex=0.0","handley=0.0","alpha=!"+envname])
# send("/event",["target="+envname,"type=bang"])
time.sleep(10.0)

for k in range(0,10):
	print "k=",k
	for i in range(0,10):
		lfoname= "LFO%04d%04d"%(k,i)
		send("/lfo",["name="+lfoname,"center=0.0","amp=1.0","freq=%f"%(0.1*random.random())])
		send("/event",["target="+lfoname,"type=bang"])
		envname= "ENV%04d%04d"%(k,i)
		send("/env",["name="+envname,"start=1.0","end=0.0","dur=10.0"])
		send("/square",["x=!"+lfoname,"y=%f"%(2*random.random()-1.0),"hue=%f"%random.random(),"scalex=0.2","scaley=0.2","handlex=0.0","handley=0.0","alpha=!"+envname,"rotation=!"+lfoname])
		send("/event",["target="+envname,"type=bang"])
	time.sleep(1.0)


# $p /env name=EX1 start=0.0 end=0.5 dur=4.0
# $p /lfo name=LFO1 center=0.0 amp=0.5 freq=1.0
# $p /env name=EA1 start=1.0 end=0.0 dur=8.0
# 
# # $p /event target=EX1 type=bang at=5.0
# # $p /event target=LFO1 type=bang at=5.0
# # $p /event target=EA1 type=bang at=5.0
# 
# # $p /event target=Uniq0 type=bang at=5.0
# 
# $p /run
# 
# sleep 2
# $p /square x=!EX1 y=0.5 hue=0.2 scalex=0.2 scaley=0.2 handlex=0.0 handley=0.0 alpha=1.0
# 
# # $p /square x=0.5 y=0.5 hue=0.2 scalex=0.2 scaley=0.2 handlex=0.0 handley=0.0 alpha=1.0 rotation=*LFO1
# # $p /square x=0.0 y=0.0 hue=0.2 scalex=0.2 scaley=0.2 handlex=0.5 handley=0.5 alpha=1.0 rotation=*LFO1
# 
# 
# sleep 2
# $p /stop
