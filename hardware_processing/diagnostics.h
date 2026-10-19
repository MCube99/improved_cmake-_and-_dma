// diagnostics.h
typedef enum {
    DIAG_DMA_TIMEOUT   = 1u << 0,
    DIAG_DMA_SHORT     = 1u << 1,   // write_addr - base != size
    DIAG_QUEUE_FULL    = 1u << 2,
    DIAG_BAD_SIZE      = 1u << 3,
    DIAG_MOUNT_FAIL    = 1u << 4,
    DIAG_WRITE_FAIL    = 1u << 5,
    DIAG_NO_STICK      = 1u << 6,
} diag_flag_t;

void     diag_set(uint32_t f);       // ISR-safe, sticky
uint32_t diag_take(void);            // returns and clears (background only)