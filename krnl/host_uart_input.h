#ifndef SYLE_HOST_UART_INPUT_H
#define SYLE_HOST_UART_INPUT_H
#include <stdbool.h>
#include <stdint.h>
#include "../include/syle/host_console.h"
/* Single polling sample. Never wait for a byte, alter UART configuration,
 * consume the RX register when empty, or hide a host line/overrun error. */
static inline int host_uart_poll(bool available,void *ctx,
                                uint8_t (*read8)(void *,uint16_t)) {
    if(!available||!read8)return SYLE_HOST_CONSOLE_UNAVAILABLE;
    uint8_t status=read8(ctx,0x3fd);
    if(status==0xff)return SYLE_HOST_CONSOLE_UNAVAILABLE;
    if(status&0x1e)return SYLE_HOST_CONSOLE_LINE_ERROR;
    if(!(status&1))return SYLE_HOST_CONSOLE_EMPTY;
    return read8(ctx,0x3f8);
}
#endif
