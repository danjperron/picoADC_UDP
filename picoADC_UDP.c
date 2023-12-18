
/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


/*
    Copyright (c) Dec 2023  , Daniel Perron
    Add dummy UDP push to see the maximum bandwidth of the Pico
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
#include "picoADC_UDP.h"

#define ADC_NUM 0
#define ADC_PIN (26 + ADC_NUM)
#define ADC_VREF 3.3
#define ADC_RANGE (1 << 12)
#define ADC_CONVERT (ADC_VREF / (ADC_RANGE - 1))


//  UDP SEND TO HOST IP/PORT

//#define  SEND_TO_IP  "10.11.12.135"
char RemoteIP[256];
bool RemoteIPValid=false;
#define  SEND_TO_PORT 9330
#define  RCV_PORT   9330

struct udp_pcb  * send_udp_pcb;
struct udp_pcb  * rcv_udp_pcb;

int32_t blockId=0;


// start stop control
// on stop blockId is zero
// on stop unit will send  blockId zero with no data
// when received start blockId is reset and increment with data
StartStopStruct controlBlock;


// ping packet
DummyPingStruct  DummyPing;

//  packet block  hold  ADC data and sample block id
#define BLOCK_MAX  140

SampleBlockStruct block[BLOCK_MAX];
uint16_t  scrap_AD_Value[SAMPLE_CHUNK_SIZE]; // overrun are transfered there 

// fifo pointer
uint16_t  head_block=0;
uint16_t  tail_block=0;
uint16_t  blockOverrun=0; // how many lost ADC data block
// fifo block head and tail pointer function
int getBlock(uint16_t pointer, uint16_t status)
{
  for(int loop=0;loop<BLOCK_MAX;loop++)
     {
       uint16_t  theBlock = (pointer + loop) % BLOCK_MAX;
       if(block[theBlock].status == (uint16_t) status)
            return theBlock;
     }
  return -1;
}


int getHeadBlock(uint16_t status)
{
    return( getBlock(head_block++,status));
}

int getTailBlock(uint16_t status)
{
    return( getBlock(tail_block++,status));
}

int getTotalBlock(uint16_t status)
{
  int total=0;
  for(int loop=0;loop<BLOCK_MAX;loop++)
     {
       if(block[loop].blockId==0)  // blockId==0 invalid
       continue;
       if(block[loop].status & status)
        total++;
     }
  return total;
}


// mutex for for block manipulation
static mutex_t blockMutex;

// this is the UDP send packet
err_t SendUDP(char * IP , int port, void * data, int data_size)
{
      err_t t;
      ip_addr_t   destAddr;
      ip4addr_aton(IP,&destAddr);
      struct pbuf * p = pbuf_alloc(PBUF_TRANSPORT,data_size+1,PBUF_RAM);
      char *pt = (char *) p->payload;
      memcpy(pt,data,data_size);
      pt[data_size]='\0';
      cyw43_arch_lwip_begin();
      t = udp_sendto(send_udp_pcb,p,&destAddr,port);
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


// core1
//   do ADC DMA transfer and  push data to  block via fifo
//   each block hold data structure and  SAMPLE_CHUNK_SIZE of ADC data
//   we have BLOCK_size number of block

void core1_entry()
{
   //total pile status
    uint16_t  totalPile=0;
   // pt hold the pointer of the ADC data in the current block
   uint16_t * pt;

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
   mutex_enter_blocking(&blockMutex);
   int blocknow =  getHeadBlock(BLOCK_FREE);
   block[blocknow].status = BLOCK_LOCK;
   mutex_exit(&blockMutex);

   // first block
   blockId=0;
   block[blocknow].blockId = ++blockId;
   block[blocknow].sampleCount= SAMPLE_CHUNK_SIZE;
   pt= block[blocknow].AD_Value;

   dma_channel_configure(dma_0, &c_0,
        pt,             // dst
        &adc_hw->fifo,  // src
        SAMPLE_CHUNK_SIZE,  // transfer count
        true            // start immediately
      );

  // ok we are ready let's start the ADC
   adc_run(true);

 while(1)
  {
   //  mutex lock, get next head
   mutex_enter_blocking(&blockMutex);
   int blocknext =  getHeadBlock(BLOCK_FREE);
   if(blocknext<0)
   {
     // ok overrun
     blockOverrun++;
   }
   else
   {
      block[blocknext].status = BLOCK_LOCK;
      totalPile = getTotalBlock(BLOCK_READY);
   }
   mutex_exit(&blockMutex);

   ++blockId;
   if(blocknext>=0)
   {
    pt= block[blocknext].AD_Value;
    block[blocknext].blockId = blockId;
    block[blocknext].sampleCount= SAMPLE_CHUNK_SIZE;
    block[blocknext].pilePercent = (100 * totalPile)/BLOCK_MAX;
    block[blocknext].overrunCount = blockOverrun;
   }
   else
   {
   pt = scrap_AD_Value;
   }
   // set DMA  for next transfer
   dma_channel_configure(whichDMA ? dma_0 : dma_1, whichDMA ?  &c_0 : &c_1,
        pt,             // dst
        &adc_hw->fifo,  // src
        SAMPLE_CHUNK_SIZE,  // transfer count
        false            // start immediately
      );
   // wait until dma is done
   dma_channel_wait_for_finish_blocking(whichDMA ? dma_1 : dma_0);

   // ok Transfer Done  set block to be ready to transfer
   if(blocknow>=0)
      block[blocknow].status= BLOCK_READY;
   whichDMA = !whichDMA;  // swap DMA channel
   blocknow=blocknext;
 }
}


// this routine is to find blockId in block and free block
void    setAckBlockId(uint32_t blockId)
{
   if(blockId ==0) return;  // blockId =0 invalid

   // lock using mutex
    mutex_enter_blocking(&blockMutex);

   // find blockid
   for(int loop=0;loop<BLOCK_MAX;loop++)
     if(block[loop].blockId == blockId)
       if(block[loop].status & BLOCK_READY)
         {
           block[loop].status= BLOCK_FREE;
           break;
          }
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
       block[loop].blockId = 0xffffffff;  // invalidate block id
       block[loop].packetId = SAMPLE_ID;
       block[loop].packetSize = sizeof(SampleBlockStruct);
       block[loop].status= BLOCK_FREE; // Fill all samples in block to have the Ack done.
   }
  mutex_exit(&blockMutex);
}



// this is the received UDP callback
// get control or ack from remote pc

void udp_receive_callback( void* arg,              // User argument - udp_recv `arg` parameter
                           struct udp_pcb* upcb,   // Receiving Protocol Control Block
                           struct pbuf* p,         // Pointer to Datagram
                           const ip_addr_t* addr,  // Address of sender
                           u16_t port )            // Sender port 
{
   char _RemoteIP[256];
   ip_addr_t _addr;

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
         RemoteIPValid=true;
         printf("Remote IP is %s\n",RemoteIP);
       }
       else
       {
         RemoteIPValid=false;
         UBK.control.start_stop=0;
       }
     resetBlock();
     mutex_enter_blocking(&blockMutex);
     controlBlock.start_stop= UBK.control.start_stop;
    mutex_exit(&blockMutex);
   }
  }
  pbuf_free(p);
}


int main() {
    int loop;

    // *** to do ***
    // if failed just set the led flashing differently depending of the  error

    stdio_init_all();

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
        printf("Connected.n");
        printf("IP: %s\n",ipaddr_ntoa(((const ip_addr_t *)&cyw43_state.netif[0].ip_addr)));
    }

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


   printf("start\n");

   // set dummyPing packet ID
   DummyPing.packetId = PING_ID;

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

    int CurrentBlock=-1;
    int counter=0;
    while (1) {
          if(controlBlock.start_stop && RemoteIPValid)
          {
           int blockReady = getTailBlock(BLOCK_READY);

           if((++counter % 10000)==0)
              printf("blockReady : %d /%d\n",blockReady,getTotalBlock(BLOCK_READY));
           if(blockReady>=0)
            {
              if(blockReady != CurrentBlock)
              {
              if(block[blockReady].blockId==0) continue;
//              printf("send block# %d   blockId:%u\n",blockReady,block[blockReady].blockId);
              SendUDP(RemoteIP,SEND_TO_PORT,&block[blockReady],sizeof(SampleBlockStruct));
              CurrentBlock=blockReady;
              }
            }
            sleep_us(100);
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
//                if(controlBlock.start_stop)
//                  if(RemoteIPValid)
                     {
//                         printf("broadcast\n");
                         broadcastUDP(SEND_TO_PORT,&DummyPing,sizeof(DummyPing));
                     }
            }
         cyw43_arch_poll();
        }
    return 0;
}



