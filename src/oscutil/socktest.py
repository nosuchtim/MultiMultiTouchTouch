# Echo client program
import socket

HOST = 'daring.cwi.nl'    # The remote host
PORT = 50007              # The same port as used by the server
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect(("127.0.0.1", 7778))
s.send('ONE\n')
s.close()
data1 = s.recv(1024)
print 'Received data1', repr(data1)

