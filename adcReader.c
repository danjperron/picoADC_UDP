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
  #include "picoADC_UDP.h"

  #define BUFLEN 1500
  #define NPACK 100
  #define RX_PORT 9330
  #define TX_PORT 9330


  #define CYCLE_WAIT_IP 0
  #define CYCLE_COLLECT_DATA 2
  #define CYCLE_HALT 4

  int cycle = CYCLE_WAIT_IP;


// packet struct

StartStopStruct startStopPacket;
DummyPingStruct  pingPacket;
AckBlockStruct   ackPacket;

/* simple program to output the number of byte received via UDP
   hit ctrl-c to stop and output results.

   to compile    gcc -o udp.server udp.server.c

   Copywright(C) Dec 2023, Daniel Perron   MIT license
*/


// Ack knowledge blol id packet


struct timeval startTime,endTime;

  void dieNow(char *s)
  {
    perror(s);
    exit(1);
  }

 unsigned int totalPacketSize=0;

 void ctrlC(int dummy) {
     gettimeofday(&endTime, NULL);
     int elapse_us = (int)((endTime.tv_sec-startTime.tv_sec)*1000000ULL+(endTime.tv_usec-startTime.tv_usec));
     double elapse_sec = (double) elapse_us / 1.0e6;
     printf("Total Packet Size = %u bytes in %.1f seconds \n",totalPacketSize, elapse_sec);
     printf("Transfer rate is %4.2f Mbytes/sec\n", totalPacketSize / (elapse_sec*1.0e6) );
     fflush(stdout);
     usleep(100000);
     exit(0);
  }

   uint32_t currentBlockId;
   uint32_t storeBlockId;

  int main(void)
  {
    struct sockaddr_in si_me, si_other, si_ack;
    int s, i, slen=sizeof(si_other);
    int tx_socket;
    char buf[BUFLEN];
    int count=0;

    // ctrl-c stuff
    signal(SIGINT, ctrlC);


    // packet stuff
    UnionBlockStruct * unionBlock= (UnionBlockStruct *) buf;

    if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
      dieNow("socket");

    if ((tx_socket=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
      dieNow("tx socket");

    memset((char *) &si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(RX_PORT);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s,(struct sockaddr *) &si_me, sizeof(si_me))==-1)
        dieNow("bind");

   printf("Waiting for Ping of the pico\n");

    while(1){
      // wait for data
      int nbyte =recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *) &si_other, &slen);
      if(nbyte <4)
         continue;


      switch(cycle)
     {
      case CYCLE_WAIT_IP:  // wait to get the IP
                         if(unionBlock->ping.packetId == PING_ID)
                           {
                             char str[INET_ADDRSTRLEN];
                             memset((char *) &si_ack,0, sizeof(si_ack));
                             si_ack.sin_family = AF_INET;
                             si_ack.sin_addr = si_other.sin_addr;
                             si_ack.sin_port = htons(TX_PORT);
			                 inet_ntop(AF_INET, &(si_other.sin_addr), str, INET_ADDRSTRLEN);
			                 printf("Got Pico IP:%s\n",str);
                             printf("Start Pico Capture\n");
                             si_ack.sin_family = AF_INET;
                             si_ack.sin_addr = si_other.sin_addr;
                             si_ack.sin_port = htons(TX_PORT);
                             startStopPacket.packetId= STARTSTOP_ID;
                             startStopPacket.start_stop=1;
                             sendto(tx_socket, &startStopPacket, sizeof(startStopPacket), 0,
                             (struct sockaddr *) &si_ack, sizeof(si_ack));
                             cycle=CYCLE_COLLECT_DATA;
                             count=0;
                           }
                          break;
      case CYCLE_COLLECT_DATA:  // collect data
/*                         if(unionBlock->ping.packetId == PING_ID)
                          {
                            count++;
                            if(count>3)
                            {
                            // something wrong just halt
                            printf("Something wrong halt!");
                            exit(0);
                            }
                          }
*/
                          if(nbyte >= sizeof(SampleBlockStruct))
                          {
                            if(unionBlock->sample.packetId == SAMPLE_ID)
                             {
                            ackPacket.packetId= ACK_ID;
                            ackPacket.blockId= unionBlock->sample.blockId;
                            if(ackPacket.blockId>0)
                              if(ackPacket.blockId != 0xffffffff)
                               {
	                            printf("Ack:%u\n",ackPacket.blockId);
        	                    memset((char *) &si_ack,0, sizeof(si_ack)); 
              			    si_ack.sin_family = AF_INET;
                        	    si_ack.sin_addr= si_other.sin_addr;
                          	    si_ack.sin_port = htons(TX_PORT);
                           	    sendto(tx_socket, &ackPacket, sizeof(ackPacket), 0,
                                    (struct sockaddr *) &si_ack, sizeof(si_ack));
                                }
                             }
                          }
                          break;
          }
     }
    close(s);
    return 0;
 }