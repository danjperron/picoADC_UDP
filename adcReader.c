  #include <arpa/inet.h>
  #include <netinet/in.h>
  #include <stdlib.h>
  #include <stdio.h>
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <sys/time.h>
  #include <unistd.h>
  #include <string.h>
  #include <signal.h>
  #include <pthread.h>
  #include "picoADC_UDP.h"
  #include "fifoBlock.h"

  #define BUFLEN 1500
  #define NPACK 100
  #define RX_PORT 9330
  #define TX_PORT 9330


// packet struct

StartStopStruct startStopPacket;
DummyPingStruct  pingPacket;
AckBlockStruct   ackPacket;


uint64_t TotalByte=0;
uint32_t blockId=1;

// thread function

void *  rcv_udp_thread(void *);
pthread_t rcv_udp_thread_id;
int ExitFlag=0;
struct timeval startTime,endTime,T1,T2;



void dieNow(char *s)
{
perror(s);
ExitFlag=1;
exit(1);
}


uint32_t searchForBlockId(uint32_t blockID)
{
  for(int loop=0;loop<BLOCK_MAX;loop++)
   {
      if(block[loop].status & BLOCK_READY)
        {
         if(block[loop].blockId < blockID)
          {
            // already got that block just toss it
            block[loop].status = BLOCK_FREE;
          }
         if(block[loop].blockId == blockID)
             return loop;
        }
   }
   return -1;
}



void * rcv_udp_thread(void * arg)
{
    struct sockaddr_in si_me, si_other, si_ack;
    int i, slen=sizeof(si_other);
    int rx_socket,tx_socket;
    char buf[BUFLEN];
    int count=0;
    UnionBlockStruct * unionBlock= (UnionBlockStruct *) buf;

    if ((rx_socket=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
      dieNow("socket");

    if ((tx_socket=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
      dieNow("tx socket");

    memset((char *) &si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(RX_PORT);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(rx_socket,(struct sockaddr *) &si_me, sizeof(si_me))==-1)
        dieNow("bind");

     // first wait for pico ping
     while(!ExitFlag)
     {
       int nbyte =recvfrom(rx_socket, buf, BUFLEN, 0, (struct sockaddr *) &si_other, &slen);
       if(nbyte <4)
         continue;
       if(unionBlock->ping.packetId == PING_ID)
         {
             char str[INET_ADDRSTRLEN];
             memset((char *) &si_ack,0, sizeof(si_ack));
             si_ack.sin_family = AF_INET;
             si_ack.sin_addr = si_other.sin_addr;
             si_ack.sin_port = htons(TX_PORT);
             inet_ntop(AF_INET, &(si_other.sin_addr), str, INET_ADDRSTRLEN);
             fprintf(stderr,"Got Pico IP:%s\n",str);
             fprintf(stderr,"Start Pico Capture\n");
             si_ack.sin_family = AF_INET;
             si_ack.sin_addr = si_other.sin_addr;
             si_ack.sin_port = htons(TX_PORT);
             startStopPacket.packetId= STARTSTOP_ID;
             startStopPacket.start_stop=1;
             sendto(tx_socket, &startStopPacket, sizeof(startStopPacket), 0,
             (struct sockaddr *) &si_ack, sizeof(si_ack));
             blockId=1;
             TotalByte=0;
             break;
           }
      }
      // ok collect data
      gettimeofday(&startTime, NULL);
      while(!ExitFlag)
      {
       int nbyte =recvfrom(rx_socket, buf, BUFLEN, 0, (struct sockaddr *) &si_other, &slen);
       if(nbyte < sizeof(SampleBlockStruct)) continue;
       if(unionBlock->sample.packetId != SAMPLE_ID) continue;
       if(unionBlock->sample.status != BLOCK_READY) continue;
       // we gotcSAMPLE ID  send ACK
       ackPacket.packetId= ACK_ID;
       ackPacket.blockId= unionBlock->sample.blockId;
       if(ackPacket.blockId == 0xffffffff) continue;
       if(ackPacket.blockId == 0) continue;
       memset((char *) &si_ack,0, sizeof(si_ack));
       si_ack.sin_family = AF_INET;
       si_ack.sin_addr= si_other.sin_addr;
       si_ack.sin_port = htons(TX_PORT);
       sendto(tx_socket, &ackPacket, sizeof(ackPacket), 0,
              (struct sockaddr *) &si_ack, sizeof(si_ack));
       int16_t idx = getHeadBlock(BLOCK_FREE);
       if(idx<0)
        {
         dieNow("\noverrun\n");
        }
        memcpy(&block[idx],unionBlock,sizeof(SampleBlockStruct));
      }
    close(tx_socket);
    close(rx_socket);
   pthread_exit(NULL);
}



 void ctrlC(int dummy) {
     ExitFlag=1;
     fprintf(stderr,"\n");
     usleep(100000);
     exit(0);
  }


  int main(void)
  {
    int count=0;

    // ctrl-c stuff
      signal(SIGINT, ctrlC);

    // clean block
    for(int loop=0;loop<BLOCK_MAX;loop++)
      block[loop].status=BLOCK_FREE;


    // start thread
    int ret =  pthread_create(&rcv_udp_thread_id, NULL, &rcv_udp_thread,NULL);
    if(ret !=0) dieNow("unable to create receive thread\n");

    gettimeofday(&startTime, NULL);
    gettimeofday(&T1, NULL);
    while(!ExitFlag){
        gettimeofday(&endTime,NULL);
        if((endTime.tv_sec - startTime.tv_sec)>5)
         {
           ExitFlag=1;
           break;
         }
        int16_t idx = searchForBlockId(blockId);
        int16_t totalready = getTotalBlock(BLOCK_READY);
        if(idx<0) { usleep(100); continue;}
        gettimeofday(&startTime, NULL);
        uint tbyte = block[idx].sampleCount*2;
        if(tbyte > SAMPLE_CHUNK_SIZE)
           tbyte= SAMPLE_CHUNK_SIZE;
        fwrite(block[idx].AD_Value,1,tbyte*2,stdout);
        TotalByte += (uint64_t) tbyte*2;
        gettimeofday(&T2,NULL);
        if(T2.tv_sec!= T1.tv_sec)
         {
          fprintf(stderr,"blockId: %-10lu  Total: %-10lluKB\r",blockId,TotalByte / 1000);
          T1=T2;
         }
         blockId++;
        }
    pthread_exit(NULL);
    return 0;
 }
