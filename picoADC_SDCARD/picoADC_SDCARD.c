
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
#include "hardware/clocks.h"
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
#include "hardware/rtc.h"
#include "picoADC_SDCARD.h"
#include "fifoBlock.h"
#include "pico_ntp.h"
#include "ds3231.h"

#include "f_util.h"
#include "ff.h"
#include "rtc.h"
#include "hw_config.h"


#define ADC_NUM 0
#define ADC_PIN (26 + ADC_NUM)
#define ADC_VREF 3.3
#define ADC_RANGE (1 << 12)
#define ADC_CONVERT (ADC_VREF / (ADC_RANGE - 1))

#define MY_VERSION    1
#define MY_SUBVERSION 4

void resetBlock(void);


// buffer to hold adc values from dma transfer
   uint16_t  adc_dma0[SAMPLE_CHUNK_SIZE];
   uint16_t  adc_dma1[SAMPLE_CHUNK_SIZE];


int32_t blockId=0;
int32_t previousBlockId=0;
int overrunCount=0;


// start stop control
// on stop blockId is zero
// on stop unit will send  blockId zero with no data
// when received start blockId is reset and increment with data

StartStopStruct controlBlock;

// mutex for for block manipulation
static mutex_t blockMutex;


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
          block[theBlock].sampleCount= SAMPLE_CHUNK_SIZE;
          overrunCount=0;
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
          //blockId=0;
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
  controlBlock.start_stop=1;

  mutex_exit(&blockMutex);
}



// ******* sdcard ******


bool FileValid = false;
sd_card_t *pSD = NULL;
FIL fil;
char filename[32];


void WriteToSD(void *pt, int ptSize)
{
 static  uint32_t Tlen=0;
 static  uint32_t Fcount=1;


 if(pSD==NULL)
   {
      return;
   }

if(Tlen==0)
  {
   datetime_t dt;
   rtc_get_datetime(&dt);

   sprintf(filename,"ADC_%04d%02d%02d%02d%02d_%03u.dat",dt.year,dt.month,dt.day,dt.hour,dt.min,Fcount);
   FRESULT fr = f_open(&fil, filename, FA_OPEN_APPEND | FA_WRITE);
   if (FR_OK != fr && FR_EXIST != fr)
      {
        panic("f_open(%s) error: %s (%d)\n", filename, FRESULT_str(fr), fr);
        FileValid= false;
      }
   else
   {
      Fcount++;
      printf("open %s\n",filename);
      FileValid=true;
   }
  }
  if(FileValid)
   {
     UINT ptWritten;
//     printf("write %d bytes. ",ptSize);
     f_write(&fil, pt,ptSize,&ptWritten);
     Tlen += ptSize;
//     printf("%d written. Total= %llu bytes\n",ptWritten,Tlen);
   if(Tlen > 2100000000)
     {
       f_close(&fil);
       Tlen=0;
//       printf("close %s\n",filename);
     }
   }
 }

// **************  main *************

int main() {
    int loop;
//    set_sys_clock_pll(1440000000,3,3);
//    clock_configure(clk_peri,
//                        0,
//                        CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS,
//                        160000000,160000000);
// set clock 150Mhz
    set_sys_clock_pll(1500000000,5,2);
// set clock_peri to 150MHz
    clock_configure(clk_peri,
                        0,
                        CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS,
                        150000000,150000000);
    stdio_init_all();
    time_init();

    printf("\n");
    printf("\npicoADC_SDCARD V%d.%d\n",MY_VERSION,MY_SUBVERSION);
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

    rtc_init();
    get_ntp_time();

    datetime_t  _now;

    rtc_get_datetime(&_now);
    printf("date:  %02d/%02d/%04d %02d:%02d:%02d\n",
            _now.day, _now.month, _now.year,
            _now.hour, _now.min, _now.sec);

   // rtc init
   printf("Get RTC\n");
   initDS3231();
   readDS3231Time(&_now);
   printf("date:  %02d/%02d/%04d %02d:%02d:%02d\n",
            _now.day, _now.month, _now.year,
            _now.hour, _now.min, _now.sec);
/*
   // sdcard

   gpio_set_function(2, GPIO_FUNC_SPI);
   gpio_set_function(3, GPIO_FUNC_SPI);
   gpio_set_function(4, GPIO_FUNC_SPI);

    // Chip select is active-low, so we'll initialise it to a driven-high state
    gpio_init(5);
    gpio_set_dir(5, GPIO_OUT);
    gpio_put(5, 1);

*/
    pSD = sd_get_by_num(0);
    FRESULT fr = f_mount(&pSD->fatfs, pSD->pcName, 1);
    if (FR_OK != fr) panic("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
    else
      printf("SDCard mounted!\n");

    printf("SPI clock %lu\n", spi_get_baudrate(spi0));

    // Set A/D conversion to be 200K samples/sec
    // 48Mhz / 200K => 240-1

    adc_init();
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


    // create mutex to ack block id
    mutex_init(&blockMutex);


   // from start sort block from static_block
   // set default block size and ID
   resetBlock();



    multicore_launch_core1(core1_entry);

    watchdog_enable( 0x7fffff,1);
    int CurrentBlock=-1;
    int counter=0;
    int maxPile=0;
    int currentPile=0;
    controlBlock.start_stop=1;

    while (1) {
          if(controlBlock.start_stop)
          {
           int blockReady= getTailBlock(BLOCK_READY);
           if(blockReady>=0)
               {
               if((blockId % 1000)==0)
                 printf("Block Id : %lu\n",blockId);
                watchdog_update();
                WriteToSD(&block[blockReady].AD_Value,SAMPLE_12BIT_SIZE);
                block[blockReady].status=BLOCK_FREE;
              }
           }
           else
            {
               //stop mode just update watch dog
               // every ~second . this way the other computer will know the IP
                watchdog_update();
            }
        }
    return 0;
}



