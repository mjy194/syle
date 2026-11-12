#include "console.h"
#ifndef SYLE_KERNEL
#include <efilib.h>
#endif

/* Self-authored 5x7 bitmap glyphs. Lowercase is deliberately displayed as
 * uppercase. Unsupported/control bytes display '?' (except CR/LF/TAB).
 * No OS font services, heap allocations or firmware calls in output paths. */
static const UINT8 font[96][7] = {
    ['!'-32]={4,4,4,4,4,0,4}, ['"'-32]={10,10,10,0,0,0,0},
    ['#'-32]={10,31,10,10,31,10,0}, ['$'-32]={4,15,20,14,5,30,4},
    ['%'-32]={25,25,2,4,8,19,19}, ['&'-32]={12,18,20,8,21,18,13},
    ['\''-32]={4,4,8,0,0,0,0}, ['('-32]={2,4,8,8,8,4,2},
    [')'-32]={8,4,2,2,2,4,8}, ['*'-32]={0,21,14,31,14,21,0},
    ['+'-32]={0,4,4,31,4,4,0}, [','-32]={0,0,0,0,0,4,8},
    ['-'-32]={0,0,0,31,0,0,0}, ['.'-32]={0,0,0,0,0,0,4},
    ['/'-32]={1,2,2,4,8,8,16},
    ['0'-32]={14,17,19,21,25,17,14}, ['1'-32]={4,12,4,4,4,4,14},
    ['2'-32]={14,17,1,2,4,8,31}, ['3'-32]={30,1,1,14,1,1,30},
    ['4'-32]={2,6,10,18,31,2,2}, ['5'-32]={31,16,16,30,1,1,30},
    ['6'-32]={14,16,16,30,17,17,14}, ['7'-32]={31,1,2,4,8,8,8},
    ['8'-32]={14,17,17,14,17,17,14}, ['9'-32]={14,17,17,15,1,1,14},
    [':'-32]={0,4,0,0,4,0,0}, [';'-32]={0,4,0,0,4,4,8},
    ['<'-32]={1,2,4,8,4,2,1}, ['='-32]={0,0,31,0,31,0,0},
    ['>'-32]={16,8,4,2,4,8,16}, ['?'-32]={14,17,1,2,4,0,4},
    ['@'-32]={14,17,23,21,23,16,14},
    ['A'-32]={14,17,17,31,17,17,17}, ['B'-32]={30,17,17,30,17,17,30},
    ['C'-32]={14,17,16,16,16,17,14}, ['D'-32]={30,17,17,17,17,17,30},
    ['E'-32]={31,16,16,30,16,16,31}, ['F'-32]={31,16,16,30,16,16,16},
    ['G'-32]={14,17,16,23,17,17,15}, ['H'-32]={17,17,17,31,17,17,17},
    ['I'-32]={14,4,4,4,4,4,14}, ['J'-32]={7,2,2,2,2,18,12},
    ['K'-32]={17,18,20,24,20,18,17}, ['L'-32]={16,16,16,16,16,16,31},
    ['M'-32]={17,27,21,21,17,17,17}, ['N'-32]={17,25,21,19,17,17,17},
    ['O'-32]={14,17,17,17,17,17,14}, ['P'-32]={30,17,17,30,16,16,16},
    ['Q'-32]={14,17,17,17,21,18,13}, ['R'-32]={30,17,17,30,20,18,17},
    ['S'-32]={15,16,16,14,1,1,30}, ['T'-32]={31,4,4,4,4,4,4},
    ['U'-32]={17,17,17,17,17,17,14}, ['V'-32]={17,17,17,17,17,10,4},
    ['W'-32]={17,17,17,21,21,21,10}, ['X'-32]={17,17,10,4,10,17,17},
    ['Y'-32]={17,17,10,4,4,4,4}, ['Z'-32]={31,1,2,4,8,16,31},
    ['['-32]={14,8,8,8,8,8,14}, ['\\'-32]={16,8,8,4,2,2,1},
    [']'-32]={14,2,2,2,2,2,14}, ['^'-32]={4,10,17,0,0,0,0},
    ['_'-32]={0,0,0,0,0,0,31}, ['`'-32]={8,4,2,0,0,0,0},
    ['{'-32]={2,4,4,8,4,4,2}, ['|'-32]={4,4,4,4,4,4,4},
    ['}'-32]={8,4,4,2,4,4,8}, ['~'-32]={0,0,8,21,2,0,0}
};

enum { SCALE = 2, CELL_W = 12, CELL_H = 16, MAX_DIM = 8192 };
static struct syle_framebuffer boot_fb;
static struct {
    volatile UINT32 *pixels;
    UINT32 width, height, stride, columns, rows, x, y, foreground, background;
    int bgr;
} screen;

static UINT32 color(UINT8 r, UINT8 g, UINT8 b) {
    return screen.bgr ? ((UINT32)r << 16) | ((UINT32)g << 8) | b
                      : ((UINT32)b << 16) | ((UINT32)g << 8) | r;
}
static void clear(void) {
    for (UINT32 y = 0; y < screen.height; ++y)
        for (UINT32 x = 0; x < screen.width; ++x)
            screen.pixels[(UINTN)y * screen.stride + x] = screen.background;
    screen.x = screen.y = 0;
}
void console_bind(const struct syle_framebuffer *fb) {
    screen.pixels = NULL;
    boot_fb.valid = 0;
    if (!fb || fb->valid != 1 || fb->format > 1) return;
    UINT32 w = fb->width, h = fb->height, stride = fb->stride;
    if (w < CELL_W || h < CELL_H || w > MAX_DIM || h > MAX_DIM ||
        stride < w || stride > MAX_DIM) return;
    UINT64 needed = (UINT64)stride * h * sizeof(UINT32);
    UINT64 base = fb->base;
    if (!base || (base & 3) || needed > fb->size ||
        base > (UINT64)(UINTN)-1 - needed) return;
    screen.width = w; screen.height = h; screen.stride = stride;
    screen.columns = w / CELL_W; screen.rows = h / CELL_H;
    screen.bgr = fb->format == 1;
    screen.foreground = color(225, 240, 250);
    screen.background = color(8, 18, 30);
    screen.pixels = (volatile UINT32 *)(UINTN)base;
    boot_fb = *fb;
    clear();
}
#ifndef SYLE_KERNEL
void console_init(EFI_BOOT_SERVICES *bs) {
    EFI_GUID guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
    console_bind(NULL);
    if (!bs || EFI_ERROR(uefi_call_wrapper(bs->LocateProtocol, 3, &guid, NULL, (void **)&gop))) return;
    if (!gop || !gop->Mode || !gop->Mode->Info ||
        gop->Mode->SizeOfInfo < sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION)) return;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info = gop->Mode->Info;
    if (info->PixelFormat != PixelRedGreenBlueReserved8BitPerColor &&
        info->PixelFormat != PixelBlueGreenRedReserved8BitPerColor) return;
    const struct syle_framebuffer fb = {
        .base = gop->Mode->FrameBufferBase, .size = gop->Mode->FrameBufferSize,
        .width = info->HorizontalResolution, .height = info->VerticalResolution,
        .stride = info->PixelsPerScanLine,
        .format = info->PixelFormat == PixelBlueGreenRedReserved8BitPerColor ? 1 : 0,
        .valid = 1
    };
    /* Copy metadata. Output never dereferences GOP or invokes firmware. */
    console_bind(&fb);
}
#endif
void console_snapshot(struct syle_framebuffer *out) { *out = boot_fb; }
int console_available(void) { return screen.pixels != NULL; }
static void newline(void) {
    screen.x = 0;
    if (++screen.y < screen.rows) return;
    UINT32 used_h = screen.rows * CELL_H;
    for (UINT32 y = 0; y < used_h - CELL_H; ++y)
        for (UINT32 x = 0; x < screen.width; ++x)
            screen.pixels[(UINTN)y * screen.stride + x] =
                screen.pixels[(UINTN)(y + CELL_H) * screen.stride + x];
    for (UINT32 y = used_h - CELL_H; y < used_h; ++y)
        for (UINT32 x = 0; x < screen.width; ++x)
            screen.pixels[(UINTN)y * screen.stride + x] = screen.background;
    screen.y = screen.rows - 1;
}
void console_putc(char c) {
    if (!screen.pixels) return;
    if (c == '\n') { newline(); return; }
    if (c == '\r') { screen.x = 0; return; }
    if (c == '\t') { for (unsigned i = 0; i < 4; ++i) console_putc(' '); return; }
    unsigned ch = (unsigned char)c;
    if (ch >= 'a' && ch <= 'z') ch -= 'a' - 'A';
    if (ch < 32 || ch > 126) ch = '?';
    if (screen.x == screen.columns) newline();
    for (unsigned y = 0; y < CELL_H; ++y)
        for (unsigned x = 0; x < CELL_W; ++x) {
            int ink = y < 7 * SCALE && x < 5 * SCALE &&
                      (font[ch - 32][y / SCALE] & (1u << (4 - x / SCALE)));
            UINTN offset = (UINTN)(screen.y * CELL_H + y) * screen.stride +
                           screen.x * CELL_W + x;
            screen.pixels[offset] = ink ? screen.foreground : screen.background;
        }
    ++screen.x;
}
void console_panic_style(void) {
    if (!screen.pixels) return;
    screen.foreground = color(255, 235, 225);
    screen.background = color(88, 8, 12);
    clear();
}
