import re
import time
import random
from nosuch.oscutil import *

def containsAny(str, set):
	return 1 in [c in str for c in set]

port=44444
host="localhost"
# r = OscRecipient(host,port,proto="tcp")
r = OscRecipient(host,port,proto="udp")

def send(oscaddr,oscmsg):
	print "Sending oscaddr=",oscaddr," oscmsg=",oscmsg
	r.sendosc(oscaddr,oscmsg)
	# b = createBinaryMsg(oscaddr,oscmsg)
	# r.osc_socket.sendto(b, (r.osc_host, r.osc_port))
	
#	oscmsg.append(float(s))
#	oscmsg.append(int(s))
#	oscmsg.append(s)

# send("/clear",[])
# send("/list",[])
# time.sleep(20.0)

send("/clear",[])
# time.sleep(12.0)

send("/run",[])
# send("/env",["name=FREQ","start=1.1","end=0.0","dur=20.0"])
# send("/env",["name=FREQ2","start=1.0","end=1.0","dur=20.0"])
# send("/lfo",["name=FREQ1","center=0.0","amp=0.3","freq=0.023"])
# send("/lfo",["name=FREQ2","center=0.0","amp=0.3","freq=0.02"])
# send("/lfo",["name=LFO1","center=0.0","amp=0.05","freq=!FREQ1"])
# send("/lfo",["name=LFO2","center=0.0","amp=3.14159","freq=!FREQ2"])

send("/lfo",["name=LFO3","center=-0.5","amp=0.5","freq=0.5", "tag=mytag", "dur=100.0"])
send("/lfo",["name=COLORLFO","center=0.5","amp=0.5","freq=0.5", "tag=mytag"])
send("/lfo",["name=SCALELFO","center=0.5","amp=0.5","freq=0.5", "tag=mytag"])
send("/lfo",["name=SCALELFO2","center=0.3","amp=0.3","freq=8.9", "tag=mytag"])

# send("/env",["name=A1","start=0.9","end=0.0","dur=10.0"])

send("/square",["name=SQ1","tag=foobar","x=*LFO3","y=0.0","hue=!COLORLFO","scalex=0.2","scaley=*SCALELFO","alpha=0.5","lifetime=-1.0"])
time.sleep(1.0)
send("/square",["name=SQ2","tag=foobar","x=*LFO3","y=0.0","hue=!COLORLFO","scalex=0.2","scaley=*SCALELFO2","alpha=0.5","lifetime=-1.0"])
time.sleep(1.0)
send("/event",["target=LFO3","type=bang"])
time.sleep(1.0)
send("/event",["target=SCALELFO","type=bang"])
send("/event",["target=SCALELFO2","type=bang"])
send("/apply",["/name=SQ2","hue=0.5"])
send("/apply",["/name=SQ1","hue=0.0"])

time.sleep(16.0)

# send("/square",["name=SQ1","x=!LFO1","y=!LFO2","hue=!LFO2","scalex=0.9","scaley=0.9","alpha=0.04"])

# send("/square",["name=SQ1","x=!A1","y=0.0","hue=%f"%random.random(),"scalex=0.2","scaley=0.2","alpha=!A1","vel=0.001","velang=0.2"])
send("/list",[])

# time.sleep(1)
# send("/square",["name=SQ1","x=0.2"])

sys.exit(0)

time.sleep(20.0)
send("/env",["name=A2","start=0.9","end=0.0","dur=2.0"])
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
