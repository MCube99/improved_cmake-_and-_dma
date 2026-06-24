#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "queue.h"
#include "hardware/dma.h"
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
// PROCESSING PLACE
// -----------------------------------------------------------------------------
PUBLIC void classify_packet(void) {

    uint32_t size;
    event_type_t classify_event;
    size=pio_sm_get_blocking(return_spi_pio(), return_spi_sm());
    set_size(size);

    if ((size == GARY_CODE || size == GARY_CODE - 1 || size == GARY_CODE + 1 || size == 0)) { // edge cases where due to data transmission there could be wrong things

        uint32_t status = save_and_disable_interrupts();
        pio_sm_clear_fifos(return_keyboard_pio(), return_keyboard_sm());
        keyboard_check = true; // need to save and disable interrupts so that the write i not interrupted.
        pio_interrupt_clear(return_keyboard_pio(),1);
        classify_event = EVENT_KEYBOARD_DETECTED;
        restore_interrupts_from_disabled(status);
    }

    else if(size != GARY_CODE ) {

        uint32_t status = save_and_disable_interrupts();
        pio_interrupt_clear(return_spi_pio(),0);
        pio_sm_put(return_spi_pio(),return_spi_sm(),size);
        dma_setup(size);
        classify_event = EVENT_USB_PROCESSING;
        restore_interrupts_from_disabled(status);
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
    event_type_t classify_event = EVENT_DONE;
    enqueue_interrupts(classify_event);
  } 
  else {
    event_type_t classify_event = EVENT_KEYBOARD_DETECTED;
     enqueue_interrupts(classify_event);
  }
}


