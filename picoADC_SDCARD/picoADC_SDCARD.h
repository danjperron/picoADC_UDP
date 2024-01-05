/*
    Copyright (c) Dec 2023  , Daniel Perron
    Add dummy UDP push to see the maximum bandwidth of the Pico
*/

#ifndef picoadcudp
#define picoadcudp

#define  SAMPLE_CHUNK_SIZE 2048


// on 12 bits storage is *3/2
#define  SAMPLE_BYTE_SIZE  3072
#define BLOCK_MAX 60

// on 16 bits storage is *4/2
//#define  SAMPLE_BYTE_SIZE  4096
//#define BLOCK_MAX 40 

typedef struct{
   uint64_t timeStamp;      // time_us_64() stamp
   uint8_t  AD_Value[SAMPLE_BYTE_SIZE];
} SampleBlockStruct;


typedef  union{
   uint32_t ui32;
   uint8_t  ui8[4];
}union_ui32;



#endif
