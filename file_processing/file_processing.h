// file_processing.h
#pragma once

#include "hardware_processing.h"

typedef enum
{
    MOUNT_SUCCESSFUL = 0,
    FILE_CREATED_SUCCESSFUL,
}FSUCCESS;

PUBLIC bool file_processing_main();
