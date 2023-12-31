 #include "stdio.h"
#include "stdlib.h"
#include "ff.h"
#include "diskio.h"
#include "spi_sdmmc.h"
#include "W25Q.h"
#include "usb_msc.h"
#include "hardware/rtc.h"
#include "inttypes.h"
#include "hardware/gpio.h"


#define SDMMC_DRV_0     0
#define W25Q_DRV_1      1
#define USB_MSC_DRV_2   2

sdmmc_data_t *pSDMMC=NULL;
w25q_data_t *pW25Q = NULL;
usb_msc_t    *pUSB_MSC=NULL;

//==================//
DSTATUS disk_initialize (BYTE drv){
    DSTATUS stat;
    switch (drv) {
        case SDMMC_DRV_0:
            if (pSDMMC == NULL) {
                pSDMMC = (sdmmc_data_t*)malloc(sizeof(sdmmc_data_t));
                pSDMMC->csPin = SDMMC_PIN_CS;
                pSDMMC->spiPort = SDMMC_SPI_PORT;
                pSDMMC->spiInit=false;
                pSDMMC->sectSize=512;
#ifdef __SPI_SDMMC_DMA
                pSDMMC->dmaInit=false;
#endif
    }
            stat = sdmmc_disk_initialize(pSDMMC);
            return stat;
        break;
        case W25Q_DRV_1:
        if (pW25Q == NULL) {
            pW25Q = (w25q_data_t*)malloc(sizeof(w25q_data_t));
            pW25Q->spiInit=false;
            pW25Q->Stat=STA_NOINIT;
        }
		stat = w25q_disk_initialize(W25Q_SPI_PORT, W25Q_PIN_CS, pW25Q); 
		return stat;
		
	    break;
        case USB_MSC_DRV_2:
        if (pUSB_MSC == NULL) {
            pUSB_MSC = (usb_msc_t*)malloc(sizeof(usb_msc_t));
        }
        stat = usb_msc_initialize(pUSB_MSC);
        return stat;
        break;
    }
    return STA_NOINIT;
 }
/*-----------------------------------------------------------------------*/
/* Get disk status                                                       */
/*-----------------------------------------------------------------------*/
DSTATUS disk_status (BYTE drv) {
    DSTATUS stat;
    switch (drv) {
        case SDMMC_DRV_0:
            stat=  sdmmc_disk_status(pSDMMC); /* Return disk status */
            return stat;
        break;
        case W25Q_DRV_1:
            stat = pW25Q->Stat;
            return stat;
        break;
        case USB_MSC_DRV_2:
            //uint8_t dev_addr = pdrv + 1;
            return tuh_msc_mounted(pUSB_MSC->dev_addr) ? 0 : STA_NODISK;
        break;
    }
    return RES_PARERR;
	
}

/*-----------------------------------------------------------------------*/
/* Read sector(s)                                                        */
/*-----------------------------------------------------------------------*/
DRESULT disk_read (
	BYTE drv,		/* Physical drive number (0) */
	BYTE *buff,		/* Pointer to the data buffer to store read data */
	LBA_t sector,	/* Start sector number (LBA) */
	UINT count		/* Number of sectors to read (1..128) */
)
{
    DSTATUS stat;
    switch (drv) {
        case SDMMC_DRV_0:
            stat = sdmmc_disk_read(buff, sector, count, pSDMMC);
            return stat;
        break;
        case W25Q_DRV_1:
            if (pW25Q->Stat & STA_NOINIT) return RES_NOTRDY;
		    w25q_read_sector((uint32_t)sector, 0, buff, count*pW25Q->sectorSize, pW25Q);
            return pW25Q->Stat;
        break;
        case USB_MSC_DRV_2:
            stat = usb_msc_disk_read(buff, sector, count, pUSB_MSC);
            return stat;
        break;
    }
	return RES_PARERR;
}

/*-----------------------------------------------------------------------*/
/* Write sector(s)                                                       */
/*-----------------------------------------------------------------------*/
#if FF_FS_READONLY == 0
DRESULT disk_write (
	BYTE drv,			/* Physical drive number (0) */
	const BYTE *buff,	/* Ponter to the data to write */
	LBA_t sector,		/* Start sector number (LBA) */
	UINT count			/* Number of sectors to write (1..128) */
)
{
    DSTATUS stat = STA_NODISK;
    switch (drv) {
        case SDMMC_DRV_0:
            stat = sdmmc_disk_write(buff, sector, count, pSDMMC);
            return stat;
        break;
        case W25Q_DRV_1:
            stat = w25q_disk_write(buff, sector, count, pW25Q);
            return stat;
        break;
        case USB_MSC_DRV_2:
            stat = usb_msc_disk_write(buff, sector, count, pUSB_MSC);
            return stat;
        break;
    }
	return RES_PARERR;

}
#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous drive controls other than data read/write               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE drv,		/* Physical drive number (0) */
	BYTE cmd,		/* Control command code */
	void *buff		/* Pointer to the conrtol data */
)
{
    DSTATUS stat;
    switch (drv) {
        case SDMMC_DRV_0:
            stat = sdmmc_disk_ioctl(cmd, buff, pSDMMC);
            return stat;
        break;
        case W25Q_DRV_1:
            stat = w25q_disk_ioctl(cmd, buff, pW25Q);
            return stat;
        break;
        case USB_MSC_DRV_2:
            stat = usb_msc_ioctl(cmd, buff, pUSB_MSC);
            return stat;
        break;
    }
	return RES_PARERR;
}

DWORD get_fattime(void) {
    datetime_t t = {0, 0, 0, 0, 0, 0, 0};
    bool rc = rtc_get_datetime(&t);
    if (!rc) return 0;

    DWORD fattime = 0;
    // bit31:25
    // Year origin from the 1980 (0..127, e.g. 37 for 2017)
    uint8_t yr = t.year - 1980;
    fattime |= (0b01111111 & yr) << 25;
    // bit24:21
    // Month (1..12)
    uint8_t mo = t.month;
    fattime |= (0b00001111 & mo) << 21;
    // bit20:16
    // Day of the month (1..31)
    uint8_t da = t.day;
    fattime |= (0b00011111 & da) << 16;
    // bit15:11
    // Hour (0..23)
    uint8_t hr = t.hour;
    fattime |= (0b00011111 & hr) << 11;
    // bit10:5
    // Minute (0..59)
    uint8_t mi = t.min;
    fattime |= (0b00111111 & mi) << 5;
    // bit4:0
    // Second / 2 (0..29, e.g. 25 for 50)
    uint8_t sd = t.sec / 2;
    fattime |= (0b00011111 & sd);
    return fattime;
}

void led_blinking(void)
{
    static absolute_time_t  t1;
    static bool state=false;
        
    // Blink every interval ms
    if ( absolute_time_diff_us(t1, get_absolute_time()) < 100000) return; // not enough time
    t1 = get_absolute_time();
    gpio_put(LED_BLINKING_PIN, state);
    state = !state;
}

void led_blinking_off(void) {
    gpio_put(LED_BLINKING_PIN, false);
}
