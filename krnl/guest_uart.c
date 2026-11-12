#include "guest_uart.h"

void guest_uart_init(struct guest_uart *uart) {
    if (uart) *uart = (struct guest_uart){0};
}
static bool valid(const struct guest_uart *u) {
    return (u->lcr == 0 || u->lcr == 3 || u->lcr == 0x80 || u->lcr == 0x83) &&
           !(u->mcr & 0xf0) && !(u->fcr & 0x3e);
}
bool guest_uart_access(struct guest_uart *uart, unsigned offset, unsigned width,
                       bool write, uint32_t *value, bool *emit, uint8_t *byte) {
    if (!uart || !value || !emit || !byte || offset > 7 || width != 1 ||
        !valid(uart) || (write && *value > 255)) return false;
    struct guest_uart next = *uart;
    uint32_t result = *value;
    bool emitted = false;
    uint8_t output = 0;
    const bool dlab = !!(next.lcr & 0x80);
    if (write) {
        uint8_t data = (uint8_t)*value;
        switch (offset) {
        case 0:
            if (dlab) next.dll = data;
            else {
                if (next.lcr != 3) return false;
                emitted = true;
                output = data;
            }
            break;
        case 1:
            if (dlab) next.dlm = data;
            else if (data) return false;
            break;
        case 2:
            if (data & 0x38) return false;
            next.fcr = data & 0xc1;
            break;
        case 3:
            if (data != 0 && data != 3 && data != 0x80 && data != 0x83) return false;
            next.lcr = data;
            break;
        case 4:
            if (data & 0xf0) return false;
            next.mcr = data;
            break;
        case 5: case 6:
            return false;
        case 7:
            next.scr = data;
            break;
        }
    } else {
        switch (offset) {
        case 0: result = dlab ? next.dll : 0; break;
        case 1: result = dlab ? next.dlm : 0; break;
        case 2: result = (next.fcr & 1) ? 0xc1 : 1; break;
        case 3: result = next.lcr; break;
        case 4: result = next.mcr; break;
        case 5: result = 0x60; break;
        case 6: result = 0; break;
        case 7: result = next.scr; break;
        }
    }
    *uart = next;
    *value = result;
    *emit = emitted;
    *byte = output;
    return true;
}
