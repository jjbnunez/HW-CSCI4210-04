#!/usr/bin/env python

import socket
import random
import time


TCP_IP = '127.0.0.1'
TCP_PORT = 5050
BUFFER_SIZE = 1024
commands = ["LOGIN liezer\n", "LOGIN fingerbird\n", "WHO\n", "BROADCAST 12\n", "HELLO WORLD!\n", "SEND Stephen 6\n", "HELLO!\n", "LOGOUT\n"]
udp_commands = ["WHO\n"]
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((TCP_IP, TCP_PORT))

UDP_IP = "127.0.0.1"
UDP_PORT = 5051 + random.randint(1, 100)

sock = socket.socket(socket.AF_INET, # Internet
                     socket.SOCK_DGRAM) # UDP

sock.bind((UDP_IP, UDP_PORT))


for MESSAGE in commands:
	
	s.send(MESSAGE)
	print "sent message: ", MESSAGE
	if MESSAGE[:4] != "SEND" and MESSAGE[:9] != "BROADCAST": data = s.recv(BUFFER_SIZE)
	print "received data:", data
	time.sleep(3)

s.close()

for MESSAGE in udp_commands:
	print "sent udp message: ", MESSAGE
	sock.sendto(MESSAGE, (UDP_IP, 5050))
	data, addr = sock.recvfrom(1024)
	print "received message: ", data
