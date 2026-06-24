#pragma once


#include "hardware/pio.h"
#include "common_header.h"


#define BUF_LEN                               256
#define NUMBER_OF_BYTES                       BUF_LEN 
#define GARY_CODE    254                                                   

#define PICO_START          2
#define PICO_SPI_RX_PIN   ((PICO_START)      + 0)   // 2 
#define PICO_SPI_SCK_PIN  ((PICO_SPI_RX_PIN) + 1)   //3            // GPIO pin for SPI clock, same as master
#define PICO_SPI_CSN_PIN   ((PICO_SPI_RX_PIN) + 2)   //4             // GPIO pin for SPI chip select
#define PICO_SPI_TX_PIN  ((PICO_SPI_RX_PIN) + 3)   //5             // GPIO pin for SPI data to master → send from slave
#define PICO_SPI_KEYBOARD_PIN ((PICO_SPI_RX_PIN + 5))

PUBLIC void set_gpio_pins(void);
PUBLIC void gpio_set_irq_active(uint gpio, uint32_t events, bool enabled);
PUBLIC void pio_dma_setup(void);
PUBLIC void pio_keyboard_setup(void);
PUBLIC inline void dma_setup(uint32_t size);
PUBLIC void queue_init();
PUBLIC bool usb_processing_main(void);
PUBLIC void set_size(uint32_t size); 
PUBLIC void keyboard_processing_main();
PUBLIC void event_processing_main();

PUBLIC PIO const return_spi_pio();
PUBLIC PIO const return_keyboard_pio(void);
PUBLIC uint const return_spi_sm();
PUBLIC uint const return_keyboard_sm(void);
PUBLIC int const return_channel();
PUBLIC uint32_t const return_size(void); 

PUBLIC uint8_t* const give_array_address(void);
PUBLIC uint8_t* const give_array_address_for_file_writing(void);

PUBLIC volatile bool keyboard_ready;

