#pragma once
#include "hardware/pio.h"
#include "common_header.h"



PUBLIC void set_gpio_pins(void);
PUBLIC void pio_dma_setup(void);
PUBLIC void pio_keyboard_setup(void);
PUBLIC void pio_read_master_setup(void);
PUBLIC inline void dma_setup(uint32_t size);
PUBLIC void queue_init();
PUBLIC void spi_write();

PUBLIC bool usb_processing_main(void);
PUBLIC bool keyboard_processing_main();
PUBLIC bool event_processing_main();
PUBLIC void classify_packet(void);

PUBLIC PIO const return_spi_pio();
PUBLIC PIO const return_keyboard_pio(void);
PUBLIC PIO const return_master_pio(void);
PUBLIC uint const return_spi_sm();
PUBLIC uint const return_keyboard_sm(void);
PUBLIC int const return_channel();
PUBLIC uint32_t const return_size(void); 
PUBLIC uint const return_master_sm(void);



