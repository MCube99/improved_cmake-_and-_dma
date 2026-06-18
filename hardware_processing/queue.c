#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "queue.h"

#include "hardware/dma.h"

#include "hardware_processing.h"
#include "hardware/uart.h"
#include "hardware/sync.h"
#include "ring_buf.h"

// -----------------------------------------------------------------------------
// QUEUE STORAGE
// -----------------------------------------------------------------------------
#define KEYBOARD_BUFFER_SIZE 32
#define STATE_AND_QUEUE_SIZE 10

RingBufElement queue_buffer[STATE_AND_QUEUE_SIZE];
RingBuf interrupt_queue;

RingBufElement keyboard_buffer[KEYBOARD_BUFFER_SIZE];
RingBuf keyboard_queue;

RingBufElement state_buffer[STATE_AND_QUEUE_SIZE];
RingBuf state_queue;


PUBLIC void queue_init(void){
    RingBuf_ctor(&interrupt_queue, queue_buffer, STATE_AND_QUEUE_SIZE);
    RingBuf_ctor(&keyboard_queue, keyboard_buffer, KEYBOARD_BUFFER_SIZE);
    RingBuf_ctor(&state_queue, state_buffer, STATE_AND_QUEUE_SIZE);
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


///    PUBLIC bool is_queue_empty(void) {
///        bool check = RingBuf_is_full(&keyboard_queue);
///        return(check);
///    }

// -----------------------------------------------------------------------------
// PROCESSING PLACE
// -----------------------------------------------------------------------------
PUBLIC void classify_packet(void) {

    uint32_t size;
    event_type_t classify_event;
    size=pio_sm_get_blocking(return_spi_pio(), return_spi_sm());
    set_size(size);

    if ((size == GARY_CODE || size == GARY_CODE - 1 || size == GARY_CODE + 1)) { // edge cases where due to data transmission there could be wrong things
        if(pio_interrupt_get(return_keyboard_pio(),1)){
            pio_interrupt_clear(return_keyboard_pio(),1);
        }
        classify_event = EVENT_KEYBOARD_DETECTED;
    }

    else if(size > 5 ) {
        pio_sm_put(return_spi_pio(),return_spi_sm(),size);
        if(pio_interrupt_get(return_spi_pio(),0)){
            pio_interrupt_clear(return_spi_pio(),0);
        }
        classify_event = EVENT_USB_DETECTED;
    } 

    else{   //should never hit this 
        pio_sm_clear_fifos(return_spi_pio(), return_spi_sm());
        pio_sm_restart(return_spi_pio(), return_spi_sm());
    }
    

    if(!enqueue_interrupts(classify_event)) {
        return;
    }
}

PUBLIC void keyboard_processing() {
  uint8_t ch; 

  if(!dequeue_keyboard(&ch)) {
    return; }

  if(ch == '/r') {
    pio_sm_put(return_spi_pio(),return_spi_sm(),0);
    event_type_t classify_event = EVENT_PROCESSED;
    enqueue_interrupts(classify_event);
  } 
  else {
    event_type_t classify_event = EVENT_KEYBOARD_DETECTED;
     enqueue_interrupts(classify_event);
  }
}


