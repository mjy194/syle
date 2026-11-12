/* Exact production C, scripted UART register access. Not device emulation. */
#define SYLE_DIAG_HOST_TEST
#include "../../krnl/runtime.c"
#include <assert.h>
#include <stdio.h>
#include <string.h>

struct write_event { uint16_t port; uint8_t value; };
static struct write_event trace[512];
static char screen_trace[512];
static unsigned reads, writes, screen_writes, lsr_reads, scratch_reads;
static unsigned ready_after;
static uint8_t scratch_value;
static int absent, scratch_matches;

uint8_t diag_test_in8(uint16_t port) {
    ++reads;
    if (port==0x3fd) {
        ++lsr_reads;
        return absent?0xff:(ready_after && lsr_reads>=ready_after?0x20:0);
    }
    assert(port==0x3ff); ++scratch_reads;
    return scratch_matches?scratch_value:0;
}
void diag_test_out8(uint16_t port, uint8_t value) {
    assert(writes<sizeof trace/sizeof trace[0]);
    trace[writes++]=(struct write_event){port,value};
    if (port==0x3ff) scratch_value=value;
}
void console_putc(char c) {
    assert(screen_writes<sizeof screen_trace);
    screen_trace[screen_writes++]=c;
}
static void reset(void) {
    reads=writes=screen_writes=lsr_reads=scratch_reads=ready_after=0;
    scratch_value=0; absent=scratch_matches=0;
    uart_present=0; log_output_mode(0);
    memset(trace,0,sizeof trace); memset(screen_trace,0,sizeof screen_trace);
}
static void expect_bytes(const char *serial, const char *display) {
    assert(writes==strlen(serial) && screen_writes==strlen(display));
    for (unsigned i=0; i<writes; ++i)
        assert(trace[i].port==0x3f8 && trace[i].value==(uint8_t)serial[i]);
    assert(memcmp(screen_trace,display,screen_writes)==0);
}
static void probe_tests(void) {
    reset(); absent=1; serial_probe();
    assert(!uart_present && reads==1 && lsr_reads==1 && scratch_reads==0 && writes==0);
    reset(); serial_probe();
    assert(!uart_present && reads==2 && lsr_reads==1 && scratch_reads==1 && writes==1);
    assert(trace[0].port==0x3ff && trace[0].value==0x5a);
    reset(); scratch_matches=1; serial_probe();
    static const struct write_event expected[]={
        {0x3ff,0x5a},{0x3f9,0},{0x3fb,0x80},{0x3f8,1},
        {0x3f9,0},{0x3fb,3},{0x3fa,0xc7},{0x3fc,0x0b}
    };
    assert(uart_present && reads==2 && writes==sizeof expected/sizeof expected[0]);
    for (unsigned i=0; i<writes; ++i)
        assert(trace[i].port==expected[i].port && trace[i].value==expected[i].value);
    assert(screen_writes==0);
}
static void polling_tests(void) {
    reset(); uart_present=1; log_text("ABC");
    assert(reads==10000 && writes==0 && screen_writes==3 && !uart_present);
    assert(memcmp(screen_trace,"ABC",3)==0);
    ready_after=1; log_text("DEF");
    assert(reads==10000 && writes==0 && screen_writes==6 && !uart_present);
    assert(memcmp(screen_trace,"ABCDEF",6)==0);
    reset(); uart_present=1; ready_after=3; log_text("A");
    assert(reads==3 && uart_present); expect_bytes("A","A");
    reset(); uart_present=1; ready_after=10000; log_text("Z");
    assert(reads==10000 && uart_present); expect_bytes("Z","Z");
    reset(); uart_present=1; ready_after=10001; log_text("Z");
    assert(reads==10000 && !uart_present); expect_bytes("","Z");
    /* CR timeout disables the channel before the following LF attempt. */
    reset(); uart_present=1; log_text("\n");
    assert(reads==10000 && !uart_present); expect_bytes("","\n");
    reset(); ready_after=1; log_text("D");
    assert(reads==0 && !uart_present); expect_bytes("","D");
}
static void output_tests(void) {
    reset(); uart_present=1; ready_after=1; log_text("A\n");
    assert(reads==3); expect_bytes("A\r\n","A\n");
    log_output_mode(1); log_text("B\n");
    assert(reads==6); expect_bytes("A\r\nB\r\n","A\n");
    log_output_mode(2); log_text("C\n");
    assert(reads==6); expect_bytes("A\r\nB\r\n","A\nC\n");
    uart_present=0; log_output_mode(0); log_text("D");
    assert(reads==6); expect_bytes("A\r\nB\r\n","A\nC\nD");
    reset(); uart_present=1; ready_after=1; log_text("");
    assert(reads==0); expect_bytes("","");
    log_text("\r\n"); expect_bytes("\r\r\n","\r\n");
    reset(); uart_present=1; ready_after=1; serial_putc((char)0xff);
    assert(reads==1 && writes==1 && trace[0].port==0x3f8 && trace[0].value==0xff);
    assert(screen_writes==0);
    reset(); uart_present=1; ready_after=1;
    log_hex(0); log_hex(UINT64_C(0x0123456789abcdef)); log_hex(UINT64_MAX);
    expect_bytes("0x0000000000000000\r\n0x0123456789abcdef\r\n0xffffffffffffffff\r\n",
                 "0x0000000000000000\n0x0123456789abcdef\n0xffffffffffffffff\n");
    assert(reads==writes && uart_present);
}
int main(void) {
    probe_tests(); polling_tests(); output_tests();
    puts("PASS UART exact probe register sequence, bounded polling endpoints, permanent disable, CRLF bytes, modes and hex output (mocked host I/O only)");
}
