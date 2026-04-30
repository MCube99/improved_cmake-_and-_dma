#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "hardware_processing.h"

// Flags to indicate SPI status


// struct queue_type {
//     int front;          // read index
//     int rear;           // write index
//     int buffer[ BUF_LEN ];
//     SPI_STATE spi_state; // SPI status flag
// }; 

// struct queue_type;
// // Opaque pointer to queue structure
// typedef struct queue_type * Queue;

PUBLIC void queue_init();
PUBLIC uint8_t* give_array_address(void);
PUBLIC int get_queue_size();
PUBLIC inline void check_data();
PUBLIC uint8_t*  give_array_address_for_file_writing(void);
PUBLIC void set_array_index(int difference);