#include <stdio.h>
#include <stdlib.h>
#include "queue.h"
#include <string.h>
#include "hardware/sync.h"




struct queue_type {
    BYTE buffer[ BUF_LEN ];
    BYTE reset_buffer[ BUF_LEN ]; // Buffer to reset the queue, initialized to all zeros
   
}; 

static struct queue_type myQueue; // Static instance of the queue structure



// The above is for functionality where a static queue is used. This is only for this source file. 

PUBLIC void queue_init( ) {
    memset(myQueue.buffer, 0, BUF_LEN); // initialize all buffer elements to 0
    memset(myQueue.reset_buffer, 0, BUF_LEN); // initialize reset buffer elements to 0
}


PUBLIC uint8_t* give_array_address()
{
    return myQueue.buffer; // return the current position in the buffer
}

PUBLIC uint8_t* give_reset_array_address()
{
    return myQueue.reset_buffer; // return the current position in the buffer
}



PUBLIC BYTE get_queue_size()
{
    return myQueue.buffer[0]; // return the current size of the queue, which is stored in the first element of the buffer
}



// End of queue.c file
