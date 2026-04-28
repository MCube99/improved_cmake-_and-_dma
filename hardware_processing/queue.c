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
    int difference;
   
}; 

static struct queue_type myQueue; // Static instance of the queue structure



// The above is for functionality where a static queue is used. This is only for this source file. 

PUBLIC void queue_init( ) {
    memset(myQueue.buffer, 0, BUF_LEN); // initialize all buffer elements to 0
}


PUBLIC uint8_t* give_array_address(void)
{
    return (uint8_t*)&myQueue.buffer[0];
}

PUBLIC uint8_t*  give_array_address_for_file_writing()
{
    return (uint8_t*)&myQueue.buffer[1];
}


PUBLIC int get_queue_size()
{
    return(myQueue.buffer[0]);
}

PUBLIC void check_data()
{
        // Genois move on my part tbh
        int difference = dma_hw->ch[return_channel()].write_addr - (uintptr_t)&myQueue.buffer[0]; // Calculate the difference between the current DMA write address and the start of the buffer. 
        if(--difference == myQueue.buffer[0])
        {
             flag_info |= CSN_USB_EVENT; // Data has processed. This is unique to this code since Gary puts the size into the first byte of his array
        }
        else if(myQueue.buffer[0] == GARY_CODE)
        {
            flag_info |= KEYBOARD_BYTE_RECEIVED_EVENT; // Set the KEYBOARD_BYTE_RECEIVED_EVENT flag to indicate that keyboard data is ready to be read. This flag can be used in the main loop to trigger actions that should occur when keyboard data is ready, such as reading the keyboard data from the buffer and processing it accordingly.
            flag_info |= ~(CSN_USB_EVENT); // Clear the CSN_USB_EVENT flag to indicate that we are now in the state of reading keyboard data, not SPI data. This is important for the main loop to function correctly, as it relies on these flags to determine when to read from the SPI and when to read from the keyboard.
}
        }
 


 PUBLIC void set_array_index(int difference)
 {
    myQueue.difference = difference;
 }

PUBLIC uint8_t* get_array_for_readings()
{
    return(&myQueue.buffer[myQueue.difference]); //supposed to get the difference from date comma to the beginning, so we can work on that 
     
}




// End of queue.c file