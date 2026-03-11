#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "pico/stdlib.h"


#define PRIVATE static
#define PUBLIC  extern  

#define BUF_LEN                  256
#define NUMBER_OF_BYTES         BUF_LEN 

typedef uint8_t  BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;




PUBLIC void set_gpio_pins();
PUBLIC void gpio_set_irq_active(uint gpio, uint32_t events, bool enabled);
PUBLIC void pio_dma_setup(void);

PUBLIC void copy_queue_buffer( void );
PUBLIC int return_channel();


extern volatile bool spi_reading;
extern volatile bool csn_high;


