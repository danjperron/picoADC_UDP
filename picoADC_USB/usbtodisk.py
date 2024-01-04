
import binascii
import base64
import struct
import sys

count = 0
_min = 999999
_max = 0


def extractStamp(base_stamp):
    return  struct.unpack("Q",base_stamp+b'\x00\x00')[0]


oldStamp=-1
delta = 0
f=open("/dev/ttyACM0","rt")


count=0

while True:
   bline =f.readline().strip('\n')
   if len(bline) < 8:
      continue
   try:
       dline= base64.b64decode(bline)
       stamp= extractStamp(dline[0:6])
       count = count+1
       if(oldStamp >=0):
         delta =  stamp-oldStamp
         if abs(delta-3000) > 1000:
           print(stamp,"  ",count," ",delta,file=sys.stderr)
       oldStamp=stamp
       sys.stdout.buffer.write(dline[6:])
   except binascii.Error as err:
       print("error ",count,file=sys.stderr)
       continue
f.close()

