#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "queue.h"

#include "hardware/dma.h"

#include "hardware_processing.h"

// -----------------------------------------------------------------------------
// CONSTANTS
// -----------------------------------------------------------------------------

#define GARY_CODE 0xFE

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

PUBLIC packet_type_t classify_packet(void) {
    uintptr_t base = (uintptr_t)&myQueue.buffer[0];

    uintptr_t write = dma_hw->ch[return_channel()].write_addr;

    uint32_t difference = write - base;

   static uint8_t count=0;
   static uint8_t i = 0; //should never go beyind 2

    if(myQueue.buffer[i] == 0 || myQueue.buffer[i] ==1)
    {
        ++count;
        ++i;
    } 
        
    uint8_t first_usb = myQueue.buffer[i];

    // -------------------------------------------------------------------------
    // VALID SPI PACKET
    // -------------------------------------------------------------------------

    if (difference == first_usb+count+1)  {
        return PACKET_USB;
    }

    // -------------------------------------------------------------------------
    // KEYBOARD PACKET
    // -------------------------------------------------------------------------

    if (myQueue.buffer[1] == GARY_CODE||(myQueue.buffer[0] == GARY_CODE)) {
        return PACKET_KEYBOARD;
    }

    // -------------------------------------------------------------------------
    // UNKNOWN / INCOMPLETE PACKET
    // -------------------------------------------------------------------------

    return PACKET_NONE;
}