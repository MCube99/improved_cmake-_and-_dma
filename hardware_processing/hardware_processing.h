#pragma once
#include "hardware/pio.h"
#include "common_header.h"



PUBLIC void set_gpio_pins(void);
PUBLIC void pio_dma_setup(void);
PUBLIC void pio_keyboard_setup(void);
PUBLIC void __time_critical_func(dma_setup_fast)(uint32_t size);

PUBLIC void queue_init();
PUBLIC void spi_write();
PUBLIC void dma_channel_init_once(void);

PUBLIC bool usb_processing_main(void);
PUBLIC bool keyboard_processing_main();
PUBLIC void event_processing_main();
PUBLIC void __time_critical_func(classify_packet)(void);

PUBLIC PIO const return_spi_pio();
PUBLIC PIO const return_keyboard_pio(void);
PUBLIC uint const return_spi_sm();
PUBLIC uint const return_keyboard_sm(void);
PUBLIC int const return_channel();
PUBLIC uint32_t const return_size(void); 



