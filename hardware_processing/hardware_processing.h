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

#define CSN_USB_EVENT                         1<<0
#define KEYBOARD_BYTE_RECEIVED_EVENT          1<<1
#define KEYBOARD_SEND_EVENT                   1<<2
#define KEYBOARD_INVALID_CHARACTER            1<<3


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



extern volatile uint8_t flag_info;

