/*
    Copyright (c) Dec 2023  , Daniel Perron
    Add dummy UDP push to see the maximum bandwidth of the Pico
*/



#define  SAMPLE_CHUNK_SIZE 700

// UDP PACKET IDENTIFICATION LABEL
#define SAMPLE_ID    0x444F5441 // 'ATOD'
#define ACK_ID       0x214B4341 // 'ACK!'
#define STARTSTOP_ID 0x50535453 // 'STSP'
#define PING_ID      0x474E4950 // 'PING'
#define HALT_ID      0x544C4148 // 'HALT'


// BLOCK STATUS
#define BLOCK_FREE 0
#define BLOCK_LOCK 1
#define BLOCK_READY 2


// Packet Structure

typedef struct{
   uint32_t packetId;       // packet id ADID
} DummyPingStruct;

typedef struct{
   uint32_t packetId;       // packet id ADID
   uint16_t packetSize;
   uint32_t blockId;
   uint16_t sampleCount;
   uint16_t  status;         // enum blockStatus
   // statistic
   uint8_t resentCount;
   uint8_t pilePercent;
   uint16_t  AD_Value[SAMPLE_CHUNK_SIZE];
} SampleBlockStruct;

typedef struct{
   uint32_t packetId;        // packet ID AKID
   uint32_t blockId;         // 'block id'
} AckBlockStruct;

typedef struct{
   uint32_t packetId;        // packet ID STID
   uint16_t start_stop;        // packet ID
} StartStopStruct;


typedef union{
  StartStopStruct   control;
  AckBlockStruct    ack;
  DummyPingStruct   ping;
  SampleBlockStruct sample;
} UnionBlockStruct;

