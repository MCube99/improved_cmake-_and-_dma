#pragma once
// This is for common use which don' really belong in any other module/header file since its so generic

#include <stdbool.h>
#include <stdint.h>
#include "pico/stdlib.h"

#define PRIVATE static
#define PUBLIC  extern  

typedef uint8_t  BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;

#define BUF_LEN                               256
#define NUMBER_OF_BYTES                       BUF_LEN 
#define GARY_CODE                             31                                                 

extern volatile bool main_check;
extern volatile bool first_check;
extern volatile bool keyboard_check;

typedef enum
{
    EVENT_NONE=0,
    EVENT_SIZE_PACKET_RECIEVED,
    EVENT_USB_PROCESSING,
    EVENT_FILE_PROCESSING,
    EVENT_KEYBOARD_DETECTED,
    EVENT_DONE
} event_type_t;