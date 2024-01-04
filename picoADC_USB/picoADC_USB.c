
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
#include "pico/multicore.h"
#include "pico/sem.h"
#include "pico/lock_core.h"
#include <time.h>
#include "pico/util/queue.h"
#include "hardware/dma.h"
#include "hardware/watchdog.h"
#include "fifoBlock.h"


//  use serial UART   for info
//  serial usb is the printf but will output data to raspberry pi


#define UART_ID uart0
#define BAUD_RATE 115200
#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY    UART_PARITY_NONE

// We are using pins 0 and 1, but see the GPIO function select table in the
// datasheet for information on which other pins can be used.
#define UART_TX_PIN 0
#define UART_RX_PIN 1

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
   char adc_hex[SAMPLE_BYTE_SIZE*2+32];

int32_t blockId=0;


#define USE_BASE64
//#define USE_HEX12

#ifdef USE_BASE64
static char base64Table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
                                'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
                                'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                                'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
                                'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                                'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
                                'w', 'x', 'y', 'z', '0', '1', '2', '3',
                                '4', '5', '6', '7', '8', '9', '+', '/'};
static int mod_table[] = {0, 2, 1};

char *  base64_encode(char *dest, const unsigned char *data,int input_length)
{
    int output_length = 4 * ((input_length + 2) / 3);


    for (int i = 0, j = 0; i < input_length;) {

        uint32_t octet_a = i < input_length ? (unsigned char)data[i++] : 0;
        uint32_t octet_b = i < input_length ? (unsigned char)data[i++] : 0;
        uint32_t octet_c = i < input_length ? (unsigned char)data[i++] : 0;

        uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

        *(dest++) = base64Table[(triple >> 18) & 0x3F];
        *(dest++) = base64Table[(triple >> 12) & 0x3F];
        *(dest++) = base64Table[(triple >> 6) & 0x3F];
        *(dest++) = base64Table[triple  & 0x3F];
    }

    for (int i = 0; i < mod_table[input_length % 3]; i++)
        *(dest++) = '=';
    return dest;
}






#endif
const char  hexTable [16]= {'0','1','2','3','4','5','6','7',
                            '8','9','A','B','C','D','E','F'};

void SendToUSB(int idx)
{
 int loop;
 int8_t temp8; 
int16_t temp16;
 int8_t *pt8;
 int64_t temp64;
 char *pt;


// ok just 48bits microsecond for timeStamp is enough

pt = base64_encode((char *) adc_hex,(unsigned char *) &block[idx].timeStamp,6);
pt8 = block[idx].AD_Value;

   pt = base64_encode((char *) pt,(unsigned char *) pt8,SAMPLE_BYTE_SIZE);

   *(pt++) = '\n';
   *(pt++) = 0;
   puts_raw(adc_hex);
   stdio_flush();

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
    uint16_t *pt16;
    uint8_t *pt8;
    int Idx;

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
    channel_config_set_irq_quiet(&c_0, true);

    // set second DMA
    dma_channel_config c_1 = dma_channel_get_default_config(dma_1);
    channel_config_set_transfer_data_size(&c_1, DMA_SIZE_16);
    channel_config_set_read_increment(&c_1, false);
    channel_config_set_write_increment(&c_1, true);
    channel_config_set_chain_to	(&c_1,dma_0);
    channel_config_set_dreq(&c_1, DREQ_ADC);
    channel_config_set_irq_quiet(&c_1, true);
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
   whichDMA = !whichDMA;  // swap DMA channel
   dma_channel_configure(whichDMA ? dma_0 : dma_1, whichDMA ?  &c_0 : &c_1,
        whichDMA ? adc_dma0: adc_dma1,             // dst
        &adc_hw->fifo,  // src
        SAMPLE_CHUNK_SIZE,  // transfer count
        false            // start immediately
      );
   // wait until dma is done
   dma_channel_wait_for_finish_blocking(whichDMA ? dma_1 : dma_0);
   ++blockId;
   Idx = getHeadBlock();
   if(Idx>=0)
   {
      block[Idx].blockId = blockId;
      block[Idx].timeStamp = time_us_64();
      memcpy(block[Idx].AD_Value, whichDMA ? adc_dma1:adc_dma0, SAMPLE_BYTE_SIZE);
/*
      // pack to 12 bits
      pt16 = whichDMA ? adc_dma1 : adc_dma0;
      pt8  = block[Idx].AD_Value;
      uint8_t temp8;
       for( int loop=0;loop<SAMPLE_CHUNK_SIZE;loop+=2)
       {
        *(pt8++)= *pt16 & 0xff;
        temp8 = (*(pt16++) >>8) & 0xf;
        *(pt8++)=  temp8 | ((*pt16 << 4) & 0xf0);
        *(pt8++)= (*(pt16++)>>4) & 0xff;
       }
      // done move head
*/
      nextHeadBlock();
   }
  }
}


void resetBlock(void)
{
  // need to reset everything
    blockId=0;
   for(int loop=0;loop<BLOCK_MAX;loop++)
    {
       block[loop].blockId = 0;  // invalidate block id
    }
}



// **************  main *************

int main() {
    int loop;
    char sbuffer[128];
    stdio_init_all();
    stdio_set_translate_crlf(&stdio_usb, false);

    // Set up our UART with a basic baud rate.
    uart_init(UART_ID, BAUD_RATE);

    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    // Actually, we want a different speed
    // The call will return the actual baud rate selected, which will be as close as
    // possible to that requested
    int  actual_baud = uart_set_baudrate(UART_ID, BAUD_RATE); 

    sleep_ms(1600);
    sprintf(sbuffer,"\r\npicoADC_UDP V%d.%d\r\n",MY_VERSION,MY_SUBVERSION);
    uart_puts(UART_ID,sbuffer);


    sprintf(sbuffer,"sizeof SampleBlockStruct : %d\r\n",sizeof(SampleBlockStruct));
    uart_puts(UART_ID,sbuffer);

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

   // from start sort block from static_block
   // set default block size and ID
   resetBlock();
   multicore_launch_core1(core1_entry);
   while (1) {
           int blockReady= getTailBlock();
           if(blockReady>=0)
            {
                SendToUSB(blockReady);
                nextTailBlock();
            }
           }
    return 0;
}


