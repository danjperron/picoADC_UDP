/*
    Copyright (c) Dec 2023  , Daniel Perron
    Add dummy UDP push to see the maximum bandwidth of the Pico
*/

#ifndef picoadcudp
#define picoadcudp


#define  SAMPLE_CHUNK_SIZE 2700
#define  SAMPLE_BYTE_SIZE 4050


// UDP PACKET IDENTIFICATION LABEL
#define SAMPLE_ID    0x444F5441 // 'ATOD'
#define ACK_ID       0x214B4341 // 'ACK!'
#define STARTSTOP_ID 0x50535453 // 'STSP'
#define PING_ID      0x474E4950 // 'PING'
#define HALT_ID      0x544C4148 // 'HALT'



// Packet Structure

typedef  union{
   uint32_t ui32;
   uint8_t  ui8[4];
}union_ui32;


typedef struct{
   uint32_t packetId;       // packet id ADID
   uint16_t packetSize;
   uint32_t blockId;
   uint16_t sampleCount;
   uint16_t status;         // enum blockStatus
   // statistic
   uint64_t timeStamp;      // time_us_64() stamp
   uint8_t  AD_Value[SAMPLE_BYTE_SIZE]; 
} SampleBlockStruct;

typedef struct{
   uint32_t packetId;          // packet ID STID
   uint16_t start_stop;        // packet ID
   uint16_t skipOverrunBlock;  // 0=> halt and reset  1=> just forget ,
                               // nbSkipBlock++ and wait for next one.
} StartStopStruct;

#endif
