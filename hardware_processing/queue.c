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
 volatile packet_type_t current_packet = PACKET_START;

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

PUBLIC void classify_packet(void) {

    if ((return_size() != GARY_CODE )) {
        pio_sm_put(return_spi_pio(), return_spi_sm(), return_size()); 
        uint32_t status = save_and_disable_interrupts();
        current_packet = PACKET_USB;
        restore_interrupts(status);
    }

    if (return_size()== GARY_CODE ) {
        pio_sm_put(return_spi_pio(), return_spi_sm(), 0);
        current_packet = PACKET_KEYBOARD;
    }

    else {
      current_packet = PACKET_NONE;
    }
    size_byte_set = false; // reset flag for next packet

}

PUBLIC void check_usb_transfer() {

    uintptr_t base = (uintptr_t)&myQueue.buffer[0];

    uintptr_t write = dma_hw->ch[return_channel()].write_addr;

    uint32_t difference = write - base;

    if(difference ==  return_size())
    {
        usb_transfer_done = true;
        current_packet = PACKET_NONE;
    }
}