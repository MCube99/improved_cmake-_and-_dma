#ifndef HARDWARE_PROCESSING_H
#define HARDWARE_PROCESSING_H

#include "common_header.h"
#include "pico.h"

PUBLIC void set_gpio_pins(void);
PUBLIC void led_blinking_task(void);
PUBLIC void pio_miso_setup(void);
PUBLIC void pio_mosi_setup(void);
PUBLIC void dma_channel_init_once(void);
PUBLIC void __time_critical_func(dma_setup_fast)(uint32_t size);
PUBLIC void __time_critical_func(classify_packet)(void);
PUBLIC void keyboard_processing_main();
PUBLIC void event_processing_main();
PUBLIC void dma_timeout_check(void);
PUBLIC void error_processing(DIAGNOSTICS_T diagnostics);
extern volatile bool packet_recieved;
extern volatile bool not_space_check;

#endif

