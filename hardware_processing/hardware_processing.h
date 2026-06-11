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
#define GARY_CODE                   254


//////////////////////////// DEFINED GPIO FOR PIO STATE MACHINES ////////////////////////////
#define PICO_DEFAULT_SYNC_PIN     ((PICO_DEFAULT_SPI_RX_PIN) + 4)   //6             

typedef uint8_t  BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;




PUBLIC void set_gpio_pins();
PUBLIC void gpio_set_irq_active(uint gpio, uint32_t events, bool enabled);
PUBLIC void pio_dma_setup(void);
PUBLIC void pio_keyboard_setup(void);
PUBLIC void queue_init();
PUBLIC void usb_processing_main(void);
PUBLIC void set_size(uint32_t size); 
PUBLIC void keyboard_processing_main(void);

PUBLIC PIO return_spi_pio();
PUBLIC PIO return_keyboard_pio(void);
PUBLIC uint return_spi_sm();
PUBLIC uint return_keyboard_sm(void);
PUBLIC int return_channel();
PUBLIC uint32_t return_size(void); 

PUBLIC int get_queue_size();
PUBLIC uint8_t* give_array_address(void);
PUBLIC uint8_t*  give_array_address_for_file_writing(void);

// -----------------------------------------------------------------------------
// BUFFER ACCESS
// -----------------------------------------------------------------------------

PUBLIC uint8_t *give_array_address(void);

PUBLIC uint8_t *give_array_address_for_file_writing(void);

PUBLIC int get_queue_size(void);

PUBLIC volatile bool keyboard_ready;

