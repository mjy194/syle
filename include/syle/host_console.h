#ifndef SYLE_HOST_CONSOLE_ABI_H
#define SYLE_HOST_CONSOLE_ABI_H
/* Nonblocking byte input from the already-initialized host debug COM1 UART.
 * Available only in the explicit SYLE_HOST_UART_INPUT kernel build. No user
 * pointers, configurable ports, host addresses or arbitrary I/O are accepted.
 * RDI and RSI must both be zero. This is not guest UART port passthrough. */
#define SYLE_HOST_CONSOLE_RECV 4u
#define SYLE_HOST_CONSOLE_EMPTY (-1)
#define SYLE_HOST_CONSOLE_UNAVAILABLE (-2)
#define SYLE_HOST_CONSOLE_LINE_ERROR (-3)
#define SYLE_HOST_CONSOLE_INVALID (-4)
#endif
