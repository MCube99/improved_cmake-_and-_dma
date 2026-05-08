#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "hardware_processing.h"

// -----------------------------------------------------------------------------
// PACKET TYPES
// -----------------------------------------------------------------------------

typedef enum {
    PACKET_NONE,
    PACKET_USB,
    PACKET_KEYBOARD

} packet_type_t;

// -----------------------------------------------------------------------------
// QUEUE INITIALIZATION
// -----------------------------------------------------------------------------

PUBLIC void queue_init(void);

// -----------------------------------------------------------------------------
// BUFFER ACCESS
// -----------------------------------------------------------------------------

PUBLIC uint8_t *give_array_address(void);

PUBLIC uint8_t *give_array_address_for_file_writing(void);

PUBLIC int get_queue_size(void);

// -----------------------------------------------------------------------------
// PACKET CLASSIFICATION
// -----------------------------------------------------------------------------

PUBLIC packet_type_t classify_packet(void);

// -----------------------------------------------------------------------------
// OPTIONAL HELPERS
// -----------------------------------------------------------------------------

PUBLIC void set_array_index(int difference);