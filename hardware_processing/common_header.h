#pragma once
// This is for common use which don' really belong in any other module/header file since its so generic
#include <stdbool.h>
#include <stdint.h>

#define PRIVATE static
#define PUBLIC  extern  

typedef uint8_t  BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;

#define BUF_LEN                               256
#define NUMBER_OF_BYTES                       BUF_LEN 
#define GARY_CODE                             31                                                 
typedef enum
{
    EVENT_DMA_ERROR = 0,
    EVENT_SIZE_PACKET_RECIEVED,
    EVENT_FILE_PROCESSING,
    EVENT_FILE_PARSING,
    EVENT_DONE
} event_type_t;


extern volatile bool msc_ready; // from msc_app
extern volatile uint8_t device_address;
extern volatile bool not_space_check;
extern volatile bool packet_recieved;