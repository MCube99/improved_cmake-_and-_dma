#pragma once
#include "hardware/pio.h"
#include "common_header.h"


PUBLIC void set_gpio_pins(void);
PUBLIC void pio_miso_setup(void);
PUBLIC void pio_mosi_setup(void);
PUBLIC void dma_channel_init_once(void);
PUBLIC void __time_critical_func(dma_setup_fast)(uint32_t size);
PUBLIC void __time_critical_func(classify_packet)(void);
PUBLIC bool usb_processing_main(void);
PUBLIC bool keyboard_processing_main();
PUBLIC void event_processing_main();




