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
#define QUEUE_SIZE 16
RingBufElement queue_buffer[QUEUE_SIZE];
RingBuf rb_queue;

RingBufElement keyboard_buffer[16];
RingBuf keyboard_queue;

bool keyboard_check = false;

PUBLIC void queue_init(void){
    RingBuf_ctor(&rb_queue, queue_buffer, QUEUE_SIZE);
    RingBuf_ctor(&keyboard_queue, keyboard_buffer, QUEUE_SIZE);
    
    event_type_t current_event = EVENT_SIZE_PACKET_RECIEVED;
    enqueue_interrupts(current_event);
}

PUBLIC bool enqueue_interrupts(event_type_t event) {
    bool check = RingBuf_put(&rb_queue, event);
    return(check);
}

PUBLIC bool enqueue_keyboard(uint8_t letter) {
    bool check = RingBuf_put(&keyboard_queue, letter);
    return(check);
}


PUBLIC bool dequeue_interrupts(event_type_t *event) {
    bool check = RingBuf_get(&rb_queue, event);
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
    uint32_t size = pio_sm_get(return_spi_pio(), return_spi_sm());
    set_size(size);

    if ((size == GARY_CODE))
    {
        pio_sm_put( return_spi_pio(), return_spi_sm(), 0);
        keyboard_check = true;
        event_type_t classify_event = EVENT_KEYBOARD_DETECTED;
        enqueue_interrupts(classify_event);
    }
    else if(size > 0 && size < GARY_CODE)
    {
        pio_sm_put( return_spi_pio(), return_spi_sm(), size);
        keyboard_check = false;
        event_type_t classify_event = EVENT_USB_DETECTED;
        enqueue_interrupts(classify_event);
    }
}


PUBLIC void keyboard_processing(void) {
  uint8_t ch; 
  bool check = dequeue_keyboard(&ch);

  if(!check) {
    return; 
  }

  if(ch == '/r') {
    pio_sm_put(return_spi_pio(),return_spi_sm(),0);
    event_type_t classify_event = EVENT_USB_DETECTED;
    enqueue_interrupts(classify_event);

  } 

  else {
    pio_sm_put(return_spi_pio(),return_spi_sm(),ch);
  }
}