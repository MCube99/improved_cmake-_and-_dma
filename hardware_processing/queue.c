#include <stdio.h>
#include <stdlib.h>
#include "queue.h"
#include <string.h>
#include "hardware/sync.h"
#include "hardware/dma.h"
#include "hardware_processing.h"




struct queue_type {
    BYTE buffer[ BUF_LEN ];
   
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




PUBLIC BYTE get_queue_size()
{
    return myQueue.buffer[0]; // return the current size of the queue, which is stored in the first element of the buffer
}

PUBLIC bool check_data()
{
  
        int difference = dma_hw->ch[return_channel()].write_addr - (uintptr_t)&myQueue.buffer[0]; // Calculate the difference between the current DMA write address and the start of the buffer
        if(--difference == myQueue.buffer[0])
        {
            return true; // Data is ready to be processed
        }
        else
        {
            return false; // Data is not ready yet
        }
 }





// End of queue.c file