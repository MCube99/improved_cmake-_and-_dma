#pragma once
#include "hardware/pio.h"
#include "common_header.h"


PUBLIC void set_gpio_pins(void);
PUBLIC void pio_dma_setup(void);
PUBLIC void pio_mosi_setup(void);
PUBLIC void pio_miso_setup(void);
PUBLIC void queue_init();
PUBLIC void dma_channel_init_once(void);
PUBLIC void event_processing_main();
PUBLIC void __time_critical_func(classify_packet)(void);
PUBLIC void __time_critical_func(dma_setup_fast)(uint32_t size);

PUBLIC bool usb_processing_main(void);
PUBLIC bool keyboard_processing_main();

PUBLIC PIO const return_spi_pio();
PUBLIC uint const return_spi_sm();
PUBLIC PIO const return_keyboard_miso_pio(void);
PUBLIC uint const return_keyboard_miso_sm(void);
PUBLIC int const return_channel();
PUBLIC int const return_keyboard_mosi_sm(void);
PUBLIC PIO const return_keyboard_mosi_pio(void);


