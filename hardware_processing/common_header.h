#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "pico/stdlib.h"

#define PRIVATE static
#define PUBLIC  extern  

typedef uint8_t  BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;

extern volatile bool main_check;
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