#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "common_header.h"
#include "ring_buf.h"
#include "hardware_processing.h"

// -----------------------------------------------------------------------------
// PACKET TYPES
// -----------------------------------------------------------------------------



PUBLIC void classify_packet(void);
PUBLIC void keyboard_processing(void);

// -----------------------------------------------------------------------------
// QUEUE INITIALIZATION
// -----------------------------------------------------------------------------

PUBLIC void queue_init(void);
//PUBLIC bool is_queue_empty(void); 
PUBLIC bool enqueue_interrupts(event_type_t event);
PUBLIC bool enqueue_keyboard(uint8_t letter);
PUBLIC bool dequeue_interrupts(event_type_t *event);
PUBLIC bool dequeue_keyboard(uint8_t *letter);
//PUBLIC* RingBuf return_interrupt_queue();
