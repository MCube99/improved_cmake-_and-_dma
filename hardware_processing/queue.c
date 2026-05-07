#include <stdio.h>
#include <stdlib.h>
#include "queue.h"
#include <string.h>
#include "hardware/sync.h"
#include "hardware/dma.h"
#include "hardware_processing.h"
#include "hardware/pio.h"




#define GARY_CODE                           0xFE // Arbitrary value to indicate that the data in the buffer is from the keyboard, not the SPI. This is necessary since both the keyboard and SPI write into the same buffer.

struct queue_type {
    BYTE buffer[ BUF_LEN ];   
}; 

static struct queue_type myQueue; // Static instance of the queue structure



// The above is for functionality where a static queue is used. This is only for this source file. 

PUBLIC void queue_init( ) {
    memset(myQueue.buffer, 0, BUF_LEN); // initialize all buffer elements to 0
}


PUBLIC uint8_t* give_array_address(void){
    return (uint8_t*)&myQueue.buffer[0];
}

PUBLIC uint8_t*  give_array_address_for_file_writing() {
    return (uint8_t*)&myQueue.buffer[1];
}


PUBLIC int get_queue_size() {
    return(myQueue.buffer[0]);
}

PUBLIC inline void check_data(void) {
    uintptr_t base = (uintptr_t)&myQueue.buffer[0];
    uintptr_t write = dma_hw->ch[return_channel()].write_addr;

    uint32_t difference = write - base;
    uint8_t first = myQueue.buffer[0];

    if (difference == (first + 1)) {
       
        usb_check = true;
        keyboard_check = false;
        
    }
    else if (first == GARY_CODE) {

        pio_interrupt_set(return_pio(), 2);  // sets IRQ flag 1
    }
}



// End of queue.c file