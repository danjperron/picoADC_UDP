/*
    Copyright (c) Dec 2023  , Daniel Perron
    Add dummy UDP push to see the maximum bandwidth of the Pico
*/

#ifndef picoadcudp
#define picoadcudp

#define  SAMPLE_CHUNK_SIZE 2048
#define  SAMPLE_BYTE_SIZE  4096
#define BLOCK_MAX   40

typedef struct{
   uint64_t timeStamp;      // time_us_64() stamp
   uint16_t  AD_Value[SAMPLE_CHUNK_SIZE];
} SampleBlockStruct;


#endif
