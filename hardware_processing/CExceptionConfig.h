// diagnostics.h

#ifndef CEXCEPTION_CONFIG_H
#define CEXCEPTION_CONFIG_H
typedef enum {
    DIAG_DMA_TIMEOUT   = 1u << 0,
    DIAG_DMA_SHORT     = 1u << 1,   // write_addr - base != size
    DIAG_QUEUE_FULL    = 1u << 2,
    DIAG_MOUNT_FAIL    = 1u << 3,
    DIAG_WRITE_FAIL    = 1u << 4,
    DIAG_NO_STICK      = 1u << 5,
} CEXCEPTION_T;
#endif