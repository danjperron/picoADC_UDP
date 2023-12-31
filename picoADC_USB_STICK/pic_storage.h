 #ifndef _PICO_STORAGE_H_
#define _PICO_STORAGE_H_
/* used by my project */
#define SDMMC_PATH      "0:"
#define W25Q_PATH       "1:"
#define USB_MSC_PATH	"2:"
#define SPI_BAUDRATE_LOW (1000*1000)
#define SPI_BAUDRATE_HIGH (50*1000*1000)
/* =================================  */
#define  LED_BLINKING_PIN     25
void led_blinking(void);
void led_blinking_off(void);
/* =================================  */
#endif
