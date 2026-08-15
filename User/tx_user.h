#ifndef TX_USER_H
#define TX_USER_H

/* Must match the 1 kHz SysTick configured by tx_initialize_low_level.S. */
#define TX_TIMER_TICKS_PER_SECOND 1000UL

/* Stack checking for debugging (fixed fill pattern). */
#define TX_ENABLE_STACK_CHECKING

/* Let pending interrupts wake the scheduler before it re-enters its
 * interrupt-protected ready-thread check. */
// #define TX_ENABLE_WFI

#endif
