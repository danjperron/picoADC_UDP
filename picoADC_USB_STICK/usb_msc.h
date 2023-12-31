 #ifndef _USB_MSC_H_
#define _USB_MSC_H_
#include "tusb.h"

#include "ff.h"
#include "diskio.h"
#include "pico_storage.h"


typedef struct {
    uint8_t dev_addr;
    volatile bool disk_busy;
    uint8_t lun;
    uint32_t block_count;
    uint32_t block_size;
    bool     usb_mounted;
    DRESULT     Stat;

} usb_msc_t;

bool usb_msc_mounted();
DRESULT usb_msc_initialize(usb_msc_t* pusb_msc);
void wait_for_disk_io(usb_msc_t *pusb_msc);
bool disk_io_complete(uint8_t dev_addr, tuh_msc_complete_data_t const * cb_data);
DRESULT usb_msc_ioctl(BYTE cmd,	void *buff,	  usb_msc_t* pusb_msc); 
DRESULT usb_msc_disk_write(const BYTE *buff, LBA_t sector,	UINT count,   usb_msc_t *pusb_msc); 
DRESULT usb_msc_disk_read(BYTE *buff, LBA_t sector,	UINT count,   usb_msc_t *pusb_msc); 
#endif
