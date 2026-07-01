#pragma once
#include "hardware/pio.h"
#include "common_header.h"


#define PICO_START          2
#define PICO_SPI_RX_PIN   ((PICO_START)      + 0)   // 2 
#define PICO_SPI_SCK_PIN  ((PICO_SPI_RX_PIN) + 1)   //3            // GPIO pin for SPI clock, same as master
#define PICO_SPI_CSN_PIN   ((PICO_SPI_RX_PIN) + 2)   //4             // GPIO pin for SPI chip select
#define PICO_SPI_TX_PIN  ((PICO_SPI_RX_PIN) + 3)   //5             // GPIO pin for SPI data to master → send from slave
#define PICO_SPI_KEYBOARD_PIN 22

PUBLIC void set_gpio_pins(void);
PUBLIC void pio_dma_setup(void);
PUBLIC void pio_keyboard_setup(void);
PUBLIC inline void dma_setup(uint32_t size);
PUBLIC void queue_init();
PUBLIC void spi_write();

PUBLIC bool usb_processing_main(void);
PUBLIC bool keyboard_processing_main();
PUBLIC bool event_processing_main();
PUBLIC void classify_packet(void);

PUBLIC PIO const return_spi_pio();
PUBLIC PIO const return_keyboard_pio(void);
PUBLIC uint const return_spi_sm();
PUBLIC uint const return_keyboard_sm(void);
PUBLIC int const return_channel();
PUBLIC uint32_t const return_size(void); 



