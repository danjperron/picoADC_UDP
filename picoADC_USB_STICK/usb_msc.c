 #include "pico/stdlib.h"
#include "usb_msc.h"
#include "bsp/board.h"

static usb_msc_t *_pUSBMSC;

static scsi_inquiry_resp_t inquiry_resp;


bool usb_msc_mounted() {
  return _pUSBMSC->usb_mounted;
}

bool inquiry_complete_cb(uint8_t dev_addr, tuh_msc_complete_data_t const * cb_data)
{
  msc_cbw_t const* cbw = cb_data->cbw;
  msc_csw_t const* csw = cb_data->csw;

  if (csw->status != 0)
  {
    printf("Inquiry failed\r\n");
    return false;
  }

  // Print out Vendor ID, Product ID and Rev
  printf("%.8s %.16s rev %.4s\r\n", inquiry_resp.vendor_id, inquiry_resp.product_id, inquiry_resp.product_rev);

  // Get capacity of device
  _pUSBMSC->block_count = tuh_msc_get_block_count(dev_addr, cbw->lun);
  _pUSBMSC->block_size = tuh_msc_get_block_size(dev_addr, cbw->lun);

  printf("Disk Size: %lu MB\r\n", _pUSBMSC->block_count / ((1024*1024)/_pUSBMSC->block_size));

  _pUSBMSC->disk_busy=false;
  _pUSBMSC->usb_mounted=true;
  return true;
}

//------------- IMPLEMENTATION -------------//
void tuh_msc_mount_cb(uint8_t dev_addr)
{
    printf("A MassStorage device is mounted\r\n");
    _pUSBMSC->dev_addr = dev_addr;
    _pUSBMSC->lun = 0;
    tuh_msc_inquiry(_pUSBMSC->dev_addr, _pUSBMSC->lun, &inquiry_resp, inquiry_complete_cb, 0);
}

void tuh_msc_umount_cb(uint8_t dev_addr)
{
  printf("A MassStorage device is unmounted\r\n");
  _pUSBMSC->usb_mounted=false;
  f_unmount(USB_MSC_PATH);
}
/////////////

DRESULT usb_msc_initialize(usb_msc_t* pusb_msc) {
    //printf("usb_msc disk initialize\r\n");
    if (!tuh_inited()) {
      board_init();
      tuh_init(BOARD_TUH_RHPORT);
      _pUSBMSC=pusb_msc;
      _pUSBMSC->disk_busy = true;
      _pUSBMSC->usb_mounted = false;
      absolute_time_t t1=get_absolute_time();
      absolute_time_t t2;
      while (1) {
        tuh_task();
        if (_pUSBMSC->usb_mounted) {
           _pUSBMSC->Stat &= ~STA_NOINIT;
          break;
        }
        t2 = get_absolute_time();
        if (absolute_time_diff_us(t1, t2) > 10000000) {
           _pUSBMSC->Stat = STA_NOINIT;
           break;
        }
      }
    }
    _pUSBMSC->disk_busy = false;
	return _pUSBMSC->Stat;
}

void wait_for_disk_io(usb_msc_t *pusb_msc)
{
  while(pusb_msc->disk_busy)
  {
    tuh_task();
    led_blinking();
  }
  led_blinking_off();
}

bool disk_io_complete(uint8_t dev_addr, tuh_msc_complete_data_t const * cb_data)
{
  (void) dev_addr; (void) cb_data;
  _pUSBMSC->disk_busy = false; //////
  _pUSBMSC->Stat = RES_OK;
  return true;
}

DRESULT usb_msc_ioctl(
    BYTE cmd,		/* Control code */
	void *buff,		/* Buffer to send/receive control data */
    usb_msc_t* pusb_msc
)
{
    DRESULT res = RES_ERROR;

    switch ( cmd )
    {
        case CTRL_SYNC:
        // nothing to do since we do blocking
        return RES_OK;

        case GET_SECTOR_COUNT:
        //*((DWORD*) buff) = (DWORD) tuh_msc_get_block_count(pusb_msc->dev_addr, pusb_msc->lun);
        *((DWORD*) buff) = pusb_msc->block_count;
        return RES_OK;

        case GET_SECTOR_SIZE:
        //*((WORD*) buff) = (WORD) tuh_msc_get_block_size(pusb_msc->dev_addr, pusb_msc->lun);
        *((WORD*) buff) = (WORD) pusb_msc->block_size;
        return RES_OK;

        case GET_BLOCK_SIZE:
        *((DWORD*) buff) = 1;    // erase block size in units of sector size
        return RES_OK;

        default:
        return RES_PARERR;
    }
}

DRESULT usb_msc_disk_write(
	const BYTE *buff,	/* Ponter to the data to write */
	LBA_t sector,		/* Start sector number (LBA) */
	UINT count, 			/* Number of sectors to write (1..128) */
  usb_msc_t *pusb_msc
) 
{
    pusb_msc->disk_busy = true;
    tuh_msc_write10(pusb_msc->dev_addr, pusb_msc->lun, buff, sector, (uint16_t) count, disk_io_complete, 0);
    wait_for_disk_io(pusb_msc);
    return pusb_msc->Stat;
}
DRESULT usb_msc_disk_read(
	BYTE *buff,	/* Ponter to the data to write */
	LBA_t sector,		/* Start sector number (LBA) */
	UINT count, 			/* Number of sectors to write (1..128) */
  usb_msc_t *pusb_msc
) 
{
    pusb_msc->disk_busy = true;
    tuh_msc_read10(pusb_msc->dev_addr, pusb_msc->lun, buff, sector, (uint16_t) count, disk_io_complete, 0);
    wait_for_disk_io(pusb_msc);
    return pusb_msc->Stat;
}
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len)
{
}
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
}
