#include <stdio.h>
#include <stdlib.h>
#include "queue.h"
#include <string.h>
#include "hardware/sync.h"
#include "hardware/dma.h"
#include "hardware_processing.h"



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

PUBLIC inline void check_data() {
        // Genois move on my part tbh
        int difference = dma_hw->ch[return_channel()].write_addr - (uintptr_t)&myQueue.buffer[0]; // Calculate the difference between the current DMA write address and the start of the buffer. 
        if(difference == (myQueue.buffer[0] + 1))
        {
            usb_check = true; // Data has processed. This is unique to this code since Gary puts the size into the first byte of his array
             if(keyboard_check) // This means that the data in the buffer is from the keyboard, so we can start processing the keyboard data. This is necessary because we need to wait until the keyboard data is ready before we can start processing it, which could lead to data corruption or other issues if we start processing it too early.
             {
                keyboard_check = false; // Clear the keyboard_check flag to indicate that we are now in the state of reading SPI data, not keyboard data. This is important for the main loop to function correctly, as it relies on these flags to determine when to read from the SPI and when to read from the keyboard.
             }
        }

        else if(myQueue.buffer[0] == GARY_CODE)
        {
            keyboard_check = true; // Set the KEYBOARD_BYTE_RECEIVED_EVENT flag to indicate that keyboard data is ready to be read. This flag can be used in the main loop to trigger actions that should occur when keyboard data is ready, such as reading the keyboard data from the buffer and processing it accordingly.
            if(usb_check) // This means that the SPI transaction is complete and the data in the buffer is from the keyboard, so we can start processing the keyboard data. This is necessary because we need to wait until the SPI transaction is complete before we can start processing the keyboard data, which could lead to data corruption or other issues if we start processing it too early.
            {
                usb_check = false; // Clear the CSN_USB_EVENT flag to indicate that we are now in the state of reading keyboard data, not SPI data. This is important for the main loop to function correctly, as it relies on these flags to determine when to read from the SPI and when to read from the keyboard.
            }
        }           
}
 





// End of queue.c file