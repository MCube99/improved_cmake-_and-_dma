#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "queue.h"

#include "hardware/dma.h"

#include "hardware_processing.h"
#include "hardware/uart.h"
#include "hardware/sync.h"

// -----------------------------------------------------------------------------
// EXTERN VARIABLE STORAGE
// -----------------------------------------------------------------------------
 volatile packet_type_t current_packet = PACKET_NONE;

// -----------------------------------------------------------------------------
// QUEUE STORAGE
// -----------------------------------------------------------------------------

struct queue_type {
    BYTE buffer[BUF_LEN];
};

static struct queue_type myQueue;

// -----------------------------------------------------------------------------
// QUEUE INITIALIZATION
// -----------------------------------------------------------------------------

PUBLIC void queue_init(void) {
    memset(myQueue.buffer, 0, BUF_LEN);
}

// -----------------------------------------------------------------------------
// BUFFER ACCESSORS
// -----------------------------------------------------------------------------

PUBLIC uint8_t *give_array_address(void) {
    return &myQueue.buffer[0];
}

PUBLIC uint8_t *give_array_address_for_file_writing(void) {
    return &myQueue.buffer[1];
}

PUBLIC int get_queue_size(void) {
    return myQueue.buffer[0];
}

// -----------------------------------------------------------------------------
// PACKET CLASSIFICATION
// -----------------------------------------------------------------------------

PUBLIC packet_type_t classify_packet() {
    uint32_t size = 0;

     if (pio_interrupt_get(return_spi_pio(),0)) {
         pio_interrupt_clear(return_spi_pio(), return_spi_sm());
         size = pio_sm_get(return_spi_pio(), return_spi_sm());
         set_size(size);
        size = pio_sm_get(return_spi_pio(), return_spi_sm());
        set_size(size);
      }
 // i//  uint8_t size = size >> 24;
    
    uintptr_t base = (uintptr_t)&myQueue.buffer[0];

    uintptr_t write = dma_hw->ch[return_channel()].write_addr;

    uint32_t difference = write - base;

    if(size/2 == difference) {
        usb_check = true ;
    }
//   static uint8_t i = 0; //should never go beyind 2

 //  if(myQueue.buffer[i] == 0 || myQueue.buffer[i] ==1)
  //  {
//        ++i;
//    } 
//        
//    uint8_t first_usb = myQueue.buffer[i];

    // -------------------------------------------------------------------------
    // VALID SPI PACKET
    // -------------------------------------------------------------------------

    if ((size != GARY_CODE && size > 1)) {
        pio_sm_put(return_spi_pio(), return_spi_sm(), size); 
        current_packet = PACKET_USB;
        return PACKET_USB;
    }

    // -------------------------------------------------------------------------
    // KEYBOARD PACKET
    // -------------------------------------------------------------------------

    if (size == GARY_CODE || size == 0) {
        pio_sm_put(return_spi_pio(), return_spi_sm(), 0);
        current_packet = PACKET_KEYBOARD;
        return PACKET_KEYBOARD;
    }

    else {
      current_packet = PACKET_NONE;
      return PACKET_NONE;
    }

    // -------------------------------------------------------------------------
    // UNKNOWN / INCOMPLETE PACKET
    // -------------------------------------------------------------------------

    return PACKET_NONE;
}