#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "queue.h"
#include "ring_buf.h"
// -----------------------------------------------------------------------------
// QUEUE STORAGE
// -----------------------------------------------------------------------------
#define KEYBOARD_BUFFER_SIZE 32
#define STATE_AND_QUEUE_SIZE 10

struct usb_payload {
    uint32_t size;
    uint32_t difference;
    BYTE buffer[BUF_LEN];
}; // for USB PAYLOAD. This is where DMA data will go to

static struct usb_payload usbPayload;

RingBufElement queue_buffer[STATE_AND_QUEUE_SIZE];
RingBuf interrupt_queue;

RingBufElement keyboard_buffer[KEYBOARD_BUFFER_SIZE];
RingBuf keyboard_queue;


PUBLIC void queue_init(void){
    RingBuf_ctor(&interrupt_queue, queue_buffer, STATE_AND_QUEUE_SIZE);
    RingBuf_ctor(&keyboard_queue, keyboard_buffer, KEYBOARD_BUFFER_SIZE);
}

PUBLIC bool enqueue_interrupts(event_type_t event) {
    bool check = RingBuf_put(&interrupt_queue, event);
    return(check);
}


PUBLIC bool enqueue_keyboard(uint8_t letter) {
    bool check = RingBuf_put(&keyboard_queue, letter);
    return(check);
}


PUBLIC bool dequeue_interrupts(event_type_t *event) {
    bool check = RingBuf_get(&interrupt_queue, event);
    return(check);
}

PUBLIC bool dequeue_keyboard(uint8_t *letter) {
    bool check = RingBuf_get(&keyboard_queue, letter);
    return(check); 
}

// -----------------------------------------------------------------------------
// BUFFER ACCESSORS
// -----------------------------------------------------------------------------

PUBLIC uint8_t *const give_array_address(void) {
    return usbPayload.buffer;
}

PUBLIC uint8_t *const give_array_address_for_file_writing(void) {
    return &usbPayload.buffer[1];
}

PUBLIC int get_buffer_size(void) {
    return usbPayload.buffer[0];
}

PUBLIC void set_size(uint32_t size) 
{
    usbPayload.size = size;
}

PUBLIC uint32_t return_size(void) 
{
    return usbPayload.size;
}


