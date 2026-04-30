#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"


#define PRIVATE static
#define PUBLIC  extern  

#define BUF_LEN                               256
#define NUMBER_OF_BYTES                       BUF_LEN 



#define PICO_DEFAULT_START          2
#define PICO_DEFAULT_SPI_RX_PIN   ((PICO_DEFAULT_START)      + 0)   // 2 
#define PICO_DEFAULT_SPI_SCK_PIN  ((PICO_DEFAULT_SPI_RX_PIN) + 1)   //3            // GPIO pin for SPI clock, same as master
#define PICO_DEFAULT_SPI_CSN_PIN   ((PICO_DEFAULT_SPI_RX_PIN) + 2)   //4             // GPIO pin for SPI chip select
#define PICO_DEFAULT_SPI_TX_PIN  ((PICO_DEFAULT_SPI_RX_PIN) + 3)   //5             // GPIO pin for SPI data to master → send from slave


typedef uint8_t  BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;




PUBLIC void set_gpio_pins();
PUBLIC void gpio_set_irq_active(uint gpio, uint32_t events, bool enabled);
PUBLIC void pio_dma_setup(void);
PUBLIC void pio_keyboard_setup(void);
PUBLIC void usb_host_power_enable(void);

PUBLIC int return_channel();
PUBLIC PIO return_pio();
PUBLIC uint return_sm();



extern volatile bool usb_check;
extern volatile bool keyboard_check;

