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
#define QUEUE_SIZE 32
RingBufElement queue_buffer[QUEUE_SIZE];
RingBuf interrupt_queue;

RingBufElement keyboard_buffer[QUEUE_SIZE];
RingBuf keyboard_queue;

PRIVATE void pio_send(uint8_t data); 

PUBLIC void queue_init(void){
    RingBuf_ctor(&interrupt_queue, queue_buffer, QUEUE_SIZE);
    RingBuf_ctor(&keyboard_queue, keyboard_buffer, QUEUE_SIZE);
    
    event_type_t current_event = EVENT_SIZE_PACKET_RECIEVED;
    enqueue_interrupts(current_event);
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
    uint32_t size = pio_sm_get(return_spi_pio(), return_spi_sm());
    set_size(size);

    if ((size == GARY_CODE)) {
        if(pio_interrupt_get(return_keyboard_pio(),1)){
            pio_interrupt_clear(return_keyboard_pio(),1);
        }
        keyboard_check = true;
        event_type_t classify_event = EVENT_KEYBOARD_DETECTED;
        if(!enqueue_interrupts(classify_event)) {
            return;
        }
    }

    else if(size > 0 && size < GARY_CODE) {
        if(pio_interrupt_get(return_spi_pio(),0)){
            pio_interrupt_clear(return_spi_pio(),0);
        }
        event_type_t classify_event = EVENT_USB_DETECTED;
        if(!enqueue_interrupts(classify_event)){
            return;
        }
    }
}


PUBLIC void keyboard_processing(void) {
  uint8_t ch; 

  if(!dequeue_keyboard(&ch)) {
    return; 
  }

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


PUBLIC void transmit_keyboard_data() {
    RingBuf_process_all(&keyboard_queue, pio_send);
}

PRIVATE void pio_send(uint8_t data) {
    pio_sm_put(return_keyboard_pio(), return_keyboard_sm(), (uint32_t)data);
}
