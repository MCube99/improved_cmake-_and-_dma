#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "common_header.h"
#include "ring_buf.h"


// -----------------------------------------------------------------------------
// QUEUE INITIALIZATION
// -----------------------------------------------------------------------------

PUBLIC void queue_init(void);
//PUBLIC bool is_queue_empty(void); 
PUBLIC bool enqueue_interrupts(event_type_t event);
PUBLIC bool enqueue_keyboard(uint8_t letter);
PUBLIC bool dequeue_interrupts(event_type_t *event);
PUBLIC bool dequeue_keyboard(uint8_t *letter);
PUBLIC uint8_t* const give_array_address(void);
PUBLIC uint8_t* const give_array_address_for_file_writing(void);
PUBLIC void set_size(uint32_t size); 
PUBLIC uint32_t return_size(void);
PUBLIC int get_buffer_size(void);
//PUBLIC* RingBuf return_interrupt_queue();
