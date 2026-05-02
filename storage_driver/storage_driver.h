#ifndef _STORAGE_DRIVER_H_
#define _STORAGE_DRIVER_H_

#define SPI_BAUDRATE_LOW (1000*1000)
#define SPI_BAUDRATE_HIGH (40*1000*1000)

#define LED_BLINKING_PIN 25
void led_on(void);
void led_off(void);

void storage_driver_init(void);

#endif
