#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>




#define  PACKET_PERIOD_US_200K 3000
#define  PACKET_PERIOD_US_250K 2400

#define  PACKET_PERIOD_US   PACKET_PERIOD_US_200K


int goodBlock=0;
int badBlock=0;

static volatile int keepRunning = 1;

void intHandler(int dummy) {
   fprintf(stderr,"\n %d block in sequence\n",goodBlock);
   fprintf(stderr,"%d block out of sequence\n",badBlock);
   exit(0);
}

static char base64Table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
                                'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
                                'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                                'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
                                'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                                'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
                                'w', 'x', 'y', 'z', '0', '1', '2', '3',
                                '4', '5', '6', '7', '8', '9', '+', '/'};

unsigned  char invTable[256];

// assume that src is always a multiple of 4 bytes
int base64Decode(char *src,int srcSize, char *dst)
{
   int loop,idx;
   unsigned char V[4];
   for(loop=0;loop<srcSize;loop+=4)
   {
     for(idx=0;idx<4;idx++)
     {
         V[idx]= invTable[*(src++)];
         if(V[idx]==255) return 0;
     }
     *(dst++) = (char)  (V[0] << 2) + ((V[1]& 0x30) >> 4);
     *(dst++) = (char) ((V[1] & 0xf) << 4) + ((V[2] & 0x3c) >> 2);
     *(dst++) = (char) ((V[2] & 0x3) << 6) + V[3];
   }
   return 1;
}

// this version  decode base64 but from 12bits to 16bits
// simpler to convert base64 to 16bits and then split the 16bits to 12bits.
// way simpler take two character  and multiply the second by 64
int base64Decode12Bits(char *src,int srcSize, char *dst)
{
   int loop,idx;
   unsigned short V[2];
   unsigned short *pt = (unsigned short *) dst;
   for(loop=0;loop<srcSize;loop+=2)
   {
     for(idx=0;idx<2;idx++)
     {
         V[idx]= invTable[*(src++)];
         if(V[idx]==255) return 0;
     }

     *(pt++) =   V[1] + (V[0]<<6);
   }
   return 1;
}



int main(void)
{
   unsigned long long timeStampPrevious=0;
   unsigned long long timeStampDelta=0;
   unsigned long long timeStamp=0;
   int  FirstStamp=1;

   int packetSize;
   int loop;
   char buffer[4095];
   unsigned char *pt;
   unsigned char V;
   unsigned char TS[8];
   int validFlag;

   signal(SIGINT, intHandler);

   // first fill the invTable with zero
   memset(invTable,255,256);

   // fill up valid position
   for(loop=0;loop<64;loop++)
       invTable[(unsigned char) base64Table[loop]]=loop;

   while(1)
    {
       if(fgets(buffer,4095,stdin)==NULL) break;
       packetSize= strlen(buffer);
       validFlag=1;
       if(packetSize==1209)
        {
           // ok we got 12bits
           // let's decode the timestamp
           timeStamp=0;
           if(!base64Decode(buffer,4,(unsigned char *) &timeStamp))
               continue;
           // convert the rest to  16bits from pack base64 12bits
           // use the same buffer since we will never overwrite
           if(!base64Decode12Bits(&buffer[8],1200,buffer))
                continue;
           unsigned short *spt=(unsigned short *) buffer;
           if(!FirstStamp)
            {
              timeStampDelta = timeStamp-timeStampPrevious;
              int numberOfPacket =  (int)  ((timeStampDelta + PACKET_PERIOD_US/2) / PACKET_PERIOD_US);
              if(numberOfPacket > 1)
                {
                  fprintf(stderr,"Missing %d  Block\n", numberOfPacket);
                  badBlock+=numberOfPacket-1;
                }
                goodBlock++;
            }
           FirstStamp=0;
           timeStampPrevious = timeStamp;
           fwrite(buffer,1,1200,stdout);
           fflush(stdout);
        }
    }
    return 0;
}
