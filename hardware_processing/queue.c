#include <stdio.h>
#include <stdlib.h>
#include "queue.h"
#include <string.h>
#include "hardware/sync.h"




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



// End of queue.c file