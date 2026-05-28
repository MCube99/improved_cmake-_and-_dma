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
#define KEYBOARD_ESCAPE             0x0F

//////////////////////////// DEFINED GPIO FOR PIO STATE MACHINES ////////////////////////////
#define PICO_DEFAULT_SYNC_PIN     ((PICO_DEFAULT_SPI_RX_PIN) + 4)   //6             

typedef uint8_t  BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;




PUBLIC void pio_dma_setup(void);
PUBLIC void queue_init();
PUBLIC void read_first_byte(void);
//PUBLIC void spi_slave_writing(void);

PUBLIC PIO return_spi_pio();
PUBLIC uint return_spi_sm();
PUBLIC int return_channel();
PUBLIC int return_first_byte_channel(void);
PUBLIC uint32_t return_size();


PUBLIC void set_gpio_pins();
PUBLIC void set_size(uint32_t size);
PUBLIC void set_pio_irq(uint pio_irq);
PUBLIC void testIRQPIO(uint pioNum); 

PUBLIC int get_queue_size();
PUBLIC uint32_t get_size(void);
PUBLIC uint get_pio_irq(void);

PUBLIC uint8_t* give_array_address(void);
PUBLIC uint8_t*  give_array_address_for_file_writing(void);

extern volatile bool usb_check;
extern volatile bool keyboard_check;
extern volatile bool size_set;
extern volatile bool size_byte_set;
extern bool usb_transfer_done;



