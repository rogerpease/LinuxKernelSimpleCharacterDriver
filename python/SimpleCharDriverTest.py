#!/usr/bin/env python3 
#
# SimpleCharDriverTest.py
# 
# 
#
import os
from os import path

if os.geteuid() != 0:
  print("Run as root!")
  exit(1) 

def exists(path):
   try:
     os.stat(path)
   except OSError:
     return 0 
   return 1

#
# Set up two devices. 
#
#

devices = []
for i in range(2):
  devName = "/dev/simpleCharDevice"+str(i)
  if not exists(devName):
    os.system("mknod "+devName+" c 228 "+str(i)) 
  devices.append(devName)
   

assert(not (devices[0] == devices[1]))


#
# Basic test- write then read a device. 
#
#
fd1 = os.open(devices[0],os.O_RDWR)
assert(os.write(fd1,"Hello World".encode('utf-8')) == 11) 
assert("Hello" == os.read(fd1,5).decode('utf-8'))
world = os.read(fd1,6).decode('utf-8')
assert(" World" == world)  # If you fail this reads probably restart at 0. 
helloworld = os.read(fd1,13).decode('utf-8')
assert("Hello World" == helloworld) # Make sure we relooped back. 

helloworld= os.read(fd1,17).decode('utf-8')
assert("Hello World" == helloworld) # read too many characters. 
os.close(fd1)

# 
# Test a second Device. 
#
fd1 = os.open(devices[1],os.O_RDWR)
os.write(fd1,"Second Message".encode('utf-8'))
assert("Second Message" == os.read(fd1,14).decode('utf-8'))

#
# Reread first deice to make sure we didn't lose its message.
#

fd2 = os.open(devices[0],os.O_RDWR)
readBytes = os.read(fd2,20)
assert("Hello World" == readBytes.decode('utf-8'))
os.close(fd1)


#
# Read Devices in Parallel, to make sure we are holding context correctly. 
#


fd3 = os.open(devices[0],os.O_RDWR)
fd4 = os.open(devices[0],os.O_RDWR)
fd5 = os.open(devices[0],os.O_RDWR)

readBytes1a = os.read(fd3,6)
assert("Hello " == readBytes1a.decode('utf-8')) 
readBytes2a = os.read(fd4,7)
assert("Hello W" == readBytes2a.decode('utf-8')) 
readBytes3a = os.read(fd5,8)
assert("Hello Wo" == readBytes3a.decode('utf-8')) 

# Now read the rest of the messages.  This makes sure we get different read pointers for each 
# file we opened. 
readBytes1b = os.read(fd3,5)
assert("World" == readBytes1b.decode('utf-8')) 
readBytes2b = os.read(fd4,4)
assert("orld"  == readBytes2b.decode('utf-8'))
readBytes3b = os.read(fd5,3)
assert("rld"   == readBytes3b.decode('utf-8'))

os.close(fd3)
os.close(fd4)
os.close(fd5)


print ("PASS!")
