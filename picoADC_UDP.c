
/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 *  The ADC DMA is from the SDKexample
 *  it excludes the daisy chain.
 */


/*
    Copyright (c) Dec 2023  , Daniel Perron
    Add dummy UDP push to see the maximum bandwidth of the Pico

    This include the broadcast, udp transfer, fifo pile block.
    DMA daisy chain, packet ID scheme.
*/




#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "pico/binary_info.h"
#include "pico/cyw43_arch.h"
#include "pico/multicore.h"
#include "pico/sem.h"
#include "pico/lock_core.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include <time.h>
#include "pico/util/queue.h"
#include "hardware/dma.h"
#include "hardware/watchdog.h"
#include "picoADC_UDP.h"
#include "fifoBlock.h"


#define ADC_NUM 0
#define ADC_PIN (26 + ADC_NUM)
#define ADC_VREF 3.3
#define ADC_RANGE (1 << 12)
#define ADC_CONVERT (ADC_VREF / (ADC_RANGE - 1))

#define MY_VERSION    1
#define MY_SUBVERSION 3

void resetBlock(void);


// buffer to hold adc values from dma transfer
   uint16_t  adc_dma0[SAMPLE_CHUNK_SIZE];
   uint16_t  adc_dma1[SAMPLE_CHUNK_SIZE];


//  UDP SEND TO HOST IP/PORT

//#define  SEND_TO_IP  "10.11.12.135"
char RemoteIP[128];
bool RemoteIPValid=false;
#define  SEND_TO_PORT 9330
#define  RCV_PORT   9330

struct udp_pcb  * send_udp_pcb;
struct udp_pcb  * rcv_udp_pcb;



int32_t blockId=0;
int32_t previousBlockId=0;
int overrunCount=0;


// start stop control
// on stop blockId is zero
// on stop unit will send  blockId zero with no data
// when received start blockId is reset and increment with data

StartStopStruct controlBlock;


// ping packet
DummyPingStruct  DummyPing;
int pingFlag=0;

// mutex for for block manipulation
static mutex_t blockMutex;

// this is the UDP send packet
err_t SendUDP(void * data, int data_size)
{
      err_t t;
      struct pbuf * p = pbuf_alloc(PBUF_TRANSPORT,data_size+1,PBUF_RAM);
      char *pt = (char *) p->payload;
      memcpy(pt,data,data_size);
      pt[data_size]='\0';
      cyw43_arch_lwip_begin();
      t = udp_send(send_udp_pcb,p);
      cyw43_arch_lwip_end();
      pbuf_free(p);
      return t;
}

// this is the UDP send packet
err_t broadcastUDP(int port, void * data, int data_size)
{
      err_t t;
      struct udp_pcb  * datagram_pcb= udp_new();
      ip_set_option(datagram_pcb, SOF_BROADCAST);
      struct pbuf * p = pbuf_alloc(PBUF_TRANSPORT,data_size+1,PBUF_RAM);
      char *pt = (char *) p->payload;
      memcpy(pt,data,data_size);
      pt[data_size]='\0';
      cyw43_arch_lwip_begin();
      t = udp_sendto(datagram_pcb,p,IP_ADDR_BROADCAST,port);
      cyw43_arch_lwip_end();
      pbuf_free(p);
      udp_remove(datagram_pcb);
      return t;
}

//   ***********************************
//   **********    core1   *************
//   ***********************************
//   do ADC DMA transfer and  push data to  block via fifo
//   each block hold data structure and  SAMPLE_CHUNK_SIZE of ADC data
//   we have BLOCK_size number of block

void core1_entry()
{
   //  packet block  hold  ADC data and sample block id

    int sampleIdx;
    int loop;
    uint16_t  totalPile=0;
    union_ui32  ui32;
    uint16_t *pt16;
    uint8_t *pt8;
   // pt hold the pointer of the ADC data in the current block

   // this indicates which DMA are the current one
   int whichDMA = 0;

   // get 2  DMA  we will make it daizy chain
    int dma_0 = dma_claim_unused_channel(true);
    int dma_1 = dma_claim_unused_channel(true);

    // set First DMA
    dma_channel_config c_0 = dma_channel_get_default_config(dma_0);
    channel_config_set_transfer_data_size(&c_0, DMA_SIZE_16);
    channel_config_set_read_increment(&c_0, false);
    channel_config_set_write_increment(&c_0, true);
    channel_config_set_chain_to	(&c_0,dma_1);
    channel_config_set_dreq(&c_0, DREQ_ADC);

    // set second DMA
    dma_channel_config c_1 = dma_channel_get_default_config(dma_1);
    channel_config_set_transfer_data_size(&c_1, DMA_SIZE_16);
    channel_config_set_read_increment(&c_1, false);
    channel_config_set_write_increment(&c_1, true);
    channel_config_set_chain_to	(&c_1,dma_0);
    channel_config_set_dreq(&c_1, DREQ_ADC);

   // start first DMA , get head block and  lock block

   dma_channel_configure(dma_0, &c_0,
        adc_dma0,             // dst
        &adc_hw->fifo,  // src
        SAMPLE_CHUNK_SIZE,  // transfer count
        true            // start immediately
      );
  // ok we are ready let's start the ADC
   adc_run(true);

  while(1)
  {
   // set DMA  for next transfer
   dma_channel_configure(whichDMA ? dma_0 : dma_1, whichDMA ?  &c_0 : &c_1,
        whichDMA ? adc_dma0: adc_dma1,             // dst
        &adc_hw->fifo,  // src
        SAMPLE_CHUNK_SIZE,  // transfer count
        false            // start immediately
      );
   // wait until dma is done
   dma_channel_wait_for_finish_blocking(whichDMA ? dma_1 : dma_0);
   int theBlock = -1;
   if(controlBlock.start_stop)
    {
       theBlock = getHeadBlock(BLOCK_FREE);
       blockId++;
       if(theBlock>=0)
        {
          // ok Lock the block
          mutex_enter_blocking(&blockMutex);
          block[theBlock].status=BLOCK_LOCK;
          mutex_exit(&blockMutex);
          // fill the block
          block[theBlock].blockId = blockId;
          previousBlockId = blockId;
          block[theBlock].version = MY_VERSION<<8 | MY_SUBVERSION;
          block[theBlock].sampleCount= SAMPLE_CHUNK_SIZE;
          totalPile = getTotalBlock(BLOCK_READY);
          block[theBlock].overrunCount = overrunCount;
          overrunCount=0;
          block[theBlock].pilePercent =(uint8_t)  (100 * totalPile)/BLOCK_MAX;
          block[theBlock].timeStamp = time_us_64();
          // fill data
          // need to transfer adc dma to block  (16 bit to 12bit)
           pt16 = whichDMA ? adc_dma1 : adc_dma0;
           pt8  = block[theBlock].AD_Value;
           for( int loop=0;loop<SAMPLE_CHUNK_SIZE;loop+=2)
           {
            ui32.ui32 = ((uint32_t)  pt16[loop] & 0xfff) | ((((uint32_t) pt16[loop+1]) << 12) & 0xfff000);
            *(pt8++)= ui32.ui8[0];
            *(pt8++)= ui32.ui8[1];
            *(pt8++)= ui32.ui8[2];
           }
           // Done set it ready
           block[theBlock].status= BLOCK_READY;
           watchdog_update();
          }
        else
        {
             printf("\nGot Overrun  Total Free is %u\n",getTotalBlock(BLOCK_FREE));

             if(controlBlock.skipOverrunBlock)
             {
               printf("Continue! Increment OverrunCount to %d\n",++overrunCount);
             }
             else
               {
                 // ok overrun
                    controlBlock.start_stop=0;
                    printf("*******Reset\n\n");
                    watchdog_reboot(0,0,50000);
                    sleep_ms(1000000); // wait for watchdog to reboot;
                }
        }
     }
     else
        {
          blockId=0;
          previousBlockId=0;
        }
     whichDMA = !whichDMA;  // swap DMA channel
  }
}


// this routine is to find blockId in block and free block
void    setAckBlockId(uint32_t blockID)
{
   if(blockId ==0) return;  // blockId =0 invalid

   // lock using mutex
    mutex_enter_blocking(&blockMutex);

    int blockIdx = getBlockId(blockID,BLOCK_READY);
    if(blockIdx>=0)
           block[blockIdx].status= BLOCK_FREE;
  // free mutex
  mutex_exit(&blockMutex);
}


void resetBlock(void)
{
  // need to reset everything
    mutex_enter_blocking(&blockMutex);
    controlBlock.start_stop=0;
    blockId=0;
   for(int loop=0;loop<BLOCK_MAX;loop++)
    {
       block[loop].blockId = 0;  // invalidate block id
       block[loop].packetId = SAMPLE_ID;
       block[loop].packetSize = sizeof(SampleBlockStruct);
       block[loop].status= BLOCK_FREE; // Fill all samples in block to have the Ack done.
   }
  overrunCount=0;
  mutex_exit(&blockMutex);
}


// ***************  UDP RECEIVE CALLBACK  ******
// this is the received UDP callback
// get control or ack from remote pc

void udp_receive_callback( void* arg,              // User argument - udp_recv `arg` parameter
                           struct udp_pcb* upcb,   // Receiving Protocol Control Block
                           struct pbuf* p,         // Pointer to Datagram
                           const ip_addr_t* addr,  // Address of sender
                           u16_t port )            // Sender port 
{
   char _RemoteIP[256];
   static ip_addr_t _addr;

  UnionBlockStruct UBK;
  if(p->len >4)  // need at least 4bytes for id
  {
   memcpy(&UBK,p->payload,p->len);


   if(UBK.ping.packetId == PING_ID)
   {
   strcpy(_RemoteIP, ipaddr_ntoa(addr));

     printf("Got Ping from %s\n",_RemoteIP);
   }
   else if(UBK.ack.packetId == ACK_ID)
   {
//     printf("ack\n");
      if(p->len >= sizeof(AckBlockStruct))
       {

        setAckBlockId(UBK.ack.blockId);
       }
   }
   else if(UBK.control.packetId ==  STARTSTOP_ID)
   {
     strcpy(_RemoteIP, ipaddr_ntoa(addr));
     // validate IP
     if(ipaddr_aton	(_RemoteIP,&_addr))
       {
         // remote IP valid
         strcpy(RemoteIP,_RemoteIP);
         memcpy(&_addr,addr,sizeof(ip_addr_t));
         udp_disconnect(send_udp_pcb);
         udp_remove(send_udp_pcb);
         send_udp_pcb = udp_new();
	     udp_connect(send_udp_pcb, &_addr, SEND_TO_PORT);
         printf("Remote IP is %s\n",RemoteIP);
         RemoteIPValid=true;
       }
       else
       {
         RemoteIPValid=false;
         UBK.control.start_stop=0;
       }
     resetBlock();
     mutex_enter_blocking(&blockMutex);
     controlBlock.start_stop= UBK.control.start_stop;
     controlBlock.skipOverrunBlock = UBK.control.skipOverrunBlock;
    mutex_exit(&blockMutex);
   }
  }
  pbuf_free(p);
}


// **************  main *************

int main() {
    int loop;

    stdio_init_all();
    sleep_ms(1600);
    printf("\n");
    printf("\npicoADC_UDP V%d.%d\n",MY_VERSION,MY_SUBVERSION);
    sleep_ms(5000);
    // initialize wifi

    if (cyw43_arch_init()) {
        printf("Init failed!\n");
        return 1;
    }

    cyw43_pm_value(CYW43_NO_POWERSAVE_MODE,200,1,1,10);
    cyw43_arch_enable_sta_mode();

    printf("WiFi ... ");
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("failed!\n");
        return 1;
    } else {
        printf("Connected to %s .\n",WIFI_SSID);
        printf("IP: %s\n",ipaddr_ntoa(((const ip_addr_t *)&cyw43_state.netif[0].ip_addr)));
    }

    printf("sizeof SampleBlockStruct : %d\n",sizeof(SampleBlockStruct));

    adc_init();

    // Set A/D conversion to be 200K samples/sec
    // 48Mhz / 200K => 240-1
    adc_set_clkdiv(239);
    adc_gpio_init( ADC_PIN);
    adc_select_input( ADC_NUM);
    adc_fifo_setup(
        true,    // Write each completed conversion to the sample FIFO
        true,    // Enable DMA data request (DREQ)
        1,       // DREQ (and IRQ) asserted when at least 1 sample present
        false,   // We won't see the ERR bit because of 8 bit reads; disable.
        false     // Shift each sample to 8 bits when pushing to FIFO
    );

   // initialize control block
   controlBlock.packetId = STARTSTOP_ID;
   controlBlock.start_stop= 0;

   //  put invalid IP
   strcpy(RemoteIP,"");


   // set dummyPing packet ID
   DummyPing.packetId = PING_ID;
   DummyPing.version = MY_VERSION<<8 | MY_SUBVERSION;

    // create mutex to ack block id
    mutex_init(&blockMutex);


   // from start sort block from static_block
   // set default block size and ID
   resetBlock();

    // create UDP
    send_udp_pcb = udp_new();
    rcv_udp_pcb = udp_new();

    udp_bind(rcv_udp_pcb, IP_ADDR_ANY, RCV_PORT ) ;
    udp_recv(rcv_udp_pcb, udp_receive_callback, NULL ) ;

    multicore_launch_core1(core1_entry);

    watchdog_enable( 0x7fffff,1);
    int CurrentBlock=-1;
    int counter=0;
    int maxPile=0;
    int currentPile=0;
    while (1) {
          if(controlBlock.start_stop && RemoteIPValid)
          {
           int blockReady= getTailBlock(BLOCK_READY);
           if(blockReady>=0)
               currentPile = getTotalBlock(BLOCK_READY);
           if(currentPile > maxPile)
             maxPile=currentPile;

           if((++counter % 10000)==0)
            {
              printf("blockId: %d  MaxPile::%d\n",blockId,maxPile);
              maxPile=0;
            }
           if(blockReady>=0)
            {

              if((blockReady != CurrentBlock) || (time_us_32()- block[blockReady].timeStamp > 5000))  // 1500/200k ~ 7.5ms  if more than 5ms just resend
              {
                block[blockReady].timeStamp=time_us_32();
              if(block[blockReady].blockId==0)
                  {
                     block[blockReady].status=BLOCK_FREE;
                     continue;
                  }
              SendUDP(&block[blockReady],sizeof(SampleBlockStruct));


              CurrentBlock=blockReady;
              }
            }
           }
           else
            {
               //stop mode just send a dummy with blockID=0
               // every ~second . this way the other computer will know the IP
               int count=0;
               while(count<20000)
                {
                    if(controlBlock.start_stop)
                    {
//                      printf("controlBlock.start_stop=%d",controlBlock.start_stop);
                      break;
                    }
                    sleep_us(50);
                    count++;
                }
//                printf("Pile ready: %u\n",getTotalBlock(BLOCK_READY));
//                printf("Pile free: %u\n",getTotalBlock(BLOCK_FREE));
//                printf("Pile LOCK: %u\n",getTotalBlock(BLOCK_LOCK));
                if(pingFlag)
                   putchar(pingFlag == 79 ? '\n' : '.');
                else
                    printf("PING broadcast ");
                watchdog_update();
                pingFlag= ++pingFlag % 80;
                broadcastUDP(SEND_TO_PORT,&DummyPing,sizeof(DummyPing));
            }
         cyw43_arch_poll();
        }
    return 0;
}



