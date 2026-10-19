// diagnostics.c
static volatile uint32_t flags;

void diag_set(uint32_t f) {
    uint32_t s = save_and_disable_interrupts();
    flags |= f;
    restore_interrupts_from_disabled(s);
}

uint32_t diag_take(void) {
    uint32_t s = save_and_disable_interrupts();
    uint32_t f = flags; flags = 0;
    restore_interrupts_from_disabled(s);
    return f;
}