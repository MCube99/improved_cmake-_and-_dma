#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "hardware_processing.h"

// -----------------------------------------------------------------------------
// PACKET TYPES
// -----------------------------------------------------------------------------

typedef enum
{
    EVENT_SIZE_PACKET_RECIEVED=0,
    EVENT_USB_DETECTED,
    EVENT_FILE_PROCESSING,
    EVENT_KEYBOARD_DETECTED,
    EVENT_PROCESSED
} event_type_t;

extern bool keyboard_check;


PUBLIC void classify_packet(void);
PUBLIC void keyboard_processing(void);

// -----------------------------------------------------------------------------
// QUEUE INITIALIZATION
// -----------------------------------------------------------------------------

PUBLIC void queue_init(void);
PUBLIC bool enqueue_interrupts(event_type_t event);
PUBLIC bool enqueue_keyboard(uint8_t letter);
PUBLIC bool dequeue_interrupts(event_type_t *event);
PUBLIC bool dequeue_keyboard(uint8_t *letter);