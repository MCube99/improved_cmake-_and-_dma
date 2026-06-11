#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "ring_buf.h"
#include "hardware_processing.h"

// -----------------------------------------------------------------------------
// PACKET TYPES
// -----------------------------------------------------------------------------

typedef enum
{
    EVENT_NONE=0,
    EVENT_SIZE_PACKET_RECIEVED,
    EVENT_USB_DETECTED,
    EVENT_USB_PROCESSING,
    EVENT_FILE_PROCESSING,
    EVENT_KEYBOARD_DETECTED,
    EVENT_KEY_PRESSED,
    EVENT_PROCESSED
} event_type_t;

extern volatile bool keyboard_check;


PUBLIC void classify_packet(void);
PUBLIC void keyboard_processing(void);

// -----------------------------------------------------------------------------
// QUEUE INITIALIZATION
// -----------------------------------------------------------------------------

PUBLIC void queue_init(void);
PUBLIC void transmit_keyboard_data(void);
//PUBLIC bool is_queue_empty(void); 
PUBLIC bool enqueue_interrupts(event_type_t event);
PUBLIC bool enqueue_keyboard(uint8_t letter);
PUBLIC bool dequeue_interrupts(event_type_t *event);
PUBLIC bool dequeue_keyboard(uint8_t *letter);
//PUBLIC* RingBuf return_interrupt_queue();
