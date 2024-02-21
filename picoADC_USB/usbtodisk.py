#!/bin/python

import binascii
import base64
import struct
import sys
import signal
import time

count = 0
_min = 999999
_max = 0


def extractStamp(base_stamp):
    return  struct.unpack("Q",base_stamp+b'\x00\x00')[0]


oldStamp=-1
delta = 0
f=open("/dev/ttyACM0","rt")


ValidSpacing=0
InvalidSpacing=0



delay_200K = 3000
delay_250K = 2400





while True:
   try:
       bline =f.readline()
   except KeyboardInterrupt:
       break
   if bline =='':
       break
   bsize = len(bline)
   if (bsize != 1209) and (bsize != 1609):
       continue

   #extract base64
   try:
       dline= base64.b64decode(bline)
       #extract stamp
       stamp= extractStamp(dline[0:6])
       if(oldStamp >=0):
         delta =  stamp-oldStamp
         if abs(delta-delay_200K) > 100:
           print(stamp,"  ",count," ",delta,file=sys.stderr)
           InvalidSpacing+=1
         else:
           ValidSpacing+=1
       oldStamp=stamp
       #extract data
       if bsize==1209:
          #12bits mode
          b12_out=b''
          for i in range(6,906,3):
              ADC0 = (dline[i]<<4) | (dline[i+1]>>4)
              ADC1 = ((dline[i+1] & 0xf) <<8) | dline[i+2]
              b12_out += struct.pack('HH',ADC0,ADC1)
          sys.stdout.buffer.write(b12_out)
       elif bsize == 1609:
          sys.stdout.buffer.write(dline[6:])
       else:
          continue
   except KeyboardInterrupt:
       break
   except binascii.Error as err:
       print("error ",count,file=sys.stderr)
       continue


print("\nContinuous valid packet :",ValidSpacing,file=sys.stderr)
print("Missing sync packet :",InvalidSpacing,file=sys.stderr)
sys.stderr.flush()

