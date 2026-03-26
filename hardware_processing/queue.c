#include <stdio.h>
#include <stdlib.h>
#include "queue.h"
#include <string.h>
#include "hardware/sync.h"
#include "hardware/dma.h"
#include "hardware_processing.h"




struct queue_type {
    BYTE buffer[ BUF_LEN ];
    int difference;
   
}; 

static struct queue_type myQueue; // Static instance of the queue structure



// The above is for functionality where a static queue is used. This is only for this source file. 

PUBLIC void queue_init( ) {
    memset(myQueue.buffer, 0, BUF_LEN); // initialize all buffer elements to 0
}


PUBLIC uint8_t* give_array_address()
{
    return myQueue.buffer; // return the current position in the buffer
}

PUBLIC uint8_t* give_array_address_for_file_writing()
{
    return &myQueue.buffer[1];
}


PUBLIC int get_queue_size()
{
    return(myQueue.buffer[0]);
}

PUBLIC bool check_data()
{
        // Genois move on my part tbh
        int difference = dma_hw->ch[return_channel()].write_addr - (uintptr_t)&myQueue.buffer[0]; // Calculate the difference between the current DMA write address and the start of the buffer. 
        if(--difference == myQueue.buffer[0])
        {
            return true; // Data has processed. This is unique to this code since Gary puts the size into the first byte of his array
        }
        else
        {
            return false; // Error somewhere. 
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