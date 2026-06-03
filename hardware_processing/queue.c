#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "queue.h"

#include "hardware/dma.h"

#include "hardware_processing.h"
#include "hardware/uart.h"
#include "hardware/sync.h"

#define ESC 27
#define ENTER 10
#define END_OF_TEXT 3   //Ctrl+C
#define CANCEL 24       //Cancel

// -----------------------------------------------------------------------------
// EXTERN VARIABLE STORAGE
// -----------------------------------------------------------------------------
 volatile packet_type_t current_packet = PACKET_NONE;

// -----------------------------------------------------------------------------
// QUEUE STORAGE
// -----------------------------------------------------------------------------

struct queue_type {
    BYTE buffer[BUF_LEN];
    char read[40];
    uint32_t size_debug;
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
    myQueue.size_debug = get_size(); // for debugging purposes, to check the size byte that is being read from the PIO state machine and stored in the queue struct, which can help identify any issues with the size byte being read incorrectly or not being read at all, which could lead to issues with packet classification and processing if the size byte is not correct.
    if ((get_size() != GARY_CODE && get_size() > 0) ) {
            current_packet = PACKET_USB;
            usb_check = true;
            pio_sm_put(return_spi_pio(), return_spi_sm(), myQueue.size_debug ); //puts the first byte of the packet, which is the size byte, into the PIO state machine for processing 
        
    }

    if (get_size()== GARY_CODE ) {
            current_packet = PACKET_KEYBOARD;
            uint8_t size = 1;
            pio_sm_put(return_spi_pio(), return_spi_sm(), size);
        }
    

    else {
        current_packet = PACKET_NONE;
    }


    size_byte_set = false; // reset flag for next packet

}

PUBLIC void check_usb_transfer() {

    uintptr_t base = (uintptr_t)&myQueue.buffer[0];

    uint32_t write = dma_hw->ch[return_channel()].write_addr;
    for(int i = 0; i < 10; i++) {
        __asm volatile("nop");
    }

    uint32_t difference = write - base;

    if(difference ==  get_size() - 1)
    {
        usb_transfer_done = true;
        current_packet = PACKET_NONE;
    }
}

PUBLIC char* convert_to_string(const volatile uint8_t *ch) {
    myQueue.read[39] = '\0';
    static uint8_t i = 0; //recalls how many time the function is calle and stores it. 
      // Stop adding if buffer is full or an escape key is received

    if ( *ch == '\r' || i ==39 )
    {
        myQueue.read[++i] = '\0'; // Null-terminate the string at the current position
        current_packet = PACKET_KEYBOARD_PROCESSING;
        i=0;
        return myQueue.read;
    }

    if (i < 39 )  // ensure space for '\0'
     {
       myQueue.read[i++] = (char)*ch; 
     }
}

PUBLIC void send_data_to_pio_for_keyboard() {
    if(pio_interrupt_get(return_spi_pio(), 0)) {
        pio_interrupt_clear(return_spi_pio(), 0);
    }
    char *count = myQueue.read;
    while(*count != '\0') {
        pio_sm_put(return_spi_pio(), return_spi_sm(), (uint32_t)*count);
        count++;
    }
    current_packet = PACKET_NONE;
}