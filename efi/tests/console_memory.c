/* Host callbacks use the GNU-EFI declared calling convention, not firmware. */
#define GNU_EFI_USE_MS_ABI
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "../src/console.c"
static void legacy_guard_test(void) {
    UINT32 storage[40 * 32 + 2];
    for (unsigned i=0; i<40*32+2; ++i) storage[i]=0xa5a5a5a5;
    screen.pixels=storage+1;
    screen.width=36; screen.height=32; screen.stride=40;
    screen.columns=3; screen.rows=2; screen.bgr=1;
    screen.foreground=color(255,255,255); screen.background=color(8,18,30);
    clear();
    for (unsigned i=0; i<200; ++i) console_putc((char)(32+i%96));
    console_putc('\t'); console_putc('\r'); console_putc('\n');
    console_panic_style(); console_putc('P'); console_putc('A'); console_putc('N');
    assert(storage[0]==0xa5a5a5a5 && storage[40*32+1]==0xa5a5a5a5);
    for (unsigned y=0; y<32; ++y)
        for (unsigned x=36; x<40; ++x) assert(storage[1+y*40+x]==0xa5a5a5a5);
    assert(screen.background==0x58080c);
    assert(storage[1] == screen.foreground); /* Top-left of P is ink. */
    /* Exercise public validation, not just manually assembled screen state. */
    struct syle_framebuffer fb = {.base=(UINTN)(storage+1), .size=40*32*4,
        .width=36,.height=32,.stride=40,.format=0,.valid=1};
    console_bind(&fb); assert(console_available());
    console_putc('X');
    for (unsigned field=0; field<10; ++field) {
        struct syle_framebuffer bad=fb;
        switch(field) {
        case 0: bad.valid=2; break;
        case 1: bad.format=2; break;
        case 2: bad.width=11; break;
        case 3: bad.height=15; break;
        case 4: bad.stride=35; break;
        case 5: bad.size--; break;
        case 6: bad.base++; break;
        case 7: bad.base=UINT64_MAX-3; break;
        case 8: bad.width=8193; break;
        case 9: bad.stride=8193; break;
        }
        console_bind(&bad); assert(!console_available());
        console_putc('X'); console_panic_style();
    }
    fb.width=12; fb.height=16; fb.stride=12; fb.size=12*16*4;
    console_bind(&fb); assert(console_available());
    for (unsigned i=0; i<100; ++i) console_putc('X');
    assert(storage[0]==0xa5a5a5a5 && storage[40*32+1]==0xa5a5a5a5);
    screen.pixels=NULL; console_putc('X'); console_panic_style();
    assert(!console_available());
    puts("PASS: draw, wrap, scroll, tab, panic palette, framebuffer/padding bounds, disabled output");
}


/* Independent pixel expectations, deliberately not read from production font. */
static const unsigned char glyph_p[7] = {30,17,17,30,16,16,16};
static const unsigned char glyph_a[7] = {14,17,17,31,17,17,17};
static const unsigned char glyph_question[7] = {14,17,1,2,4,0,4};
enum { TEST_W = 37, TEST_H = 35, TEST_STRIDE = 40, TEST_N = TEST_STRIDE * TEST_H };
static UINT32 pixels[TEST_N + 2];
static const UINT32 guard = 0xa5a5a5a5;
static struct syle_framebuffer fixture(unsigned format) {
    for (unsigned i=0; i<TEST_N+2; ++i) pixels[i]=guard;
    return (struct syle_framebuffer){.base=(UINTN)(pixels+1), .size=TEST_N*4,
        .width=TEST_W,.height=TEST_H,.stride=TEST_STRIDE,.format=format,.valid=1};
}
static void check_guards(void) {
    assert(pixels[0]==guard && pixels[TEST_N+1]==guard);
    for (unsigned y=0; y<TEST_H; ++y)
        for (unsigned x=TEST_W; x<TEST_STRIDE; ++x)
            assert(pixels[1+y*TEST_STRIDE+x]==guard);
}
static void check_cell(unsigned cx, unsigned cy, const unsigned char rows[7],
                       UINT32 fg, UINT32 bg) {
    for (unsigned y=0; y<16; ++y)
        for (unsigned x=0; x<12; ++x) {
            int ink=y<14 && x<10 && (rows[y/2] & (1u<<(4-x/2)));
            assert(pixels[1+(cy*16+y)*TEST_STRIDE+cx*12+x]==(ink?fg:bg));
        }
}
static void check_snapshot(const struct syle_framebuffer *fb) {
    struct syle_framebuffer actual={0};
    console_snapshot(&actual);
    assert(actual.base==fb->base && actual.size==fb->size);
    assert(actual.width==fb->width && actual.height==fb->height);
    assert(actual.stride==fb->stride && actual.format==fb->format);
    assert(actual.valid==fb->valid && actual.reserved==fb->reserved);
}
static void pixels_and_controls(void) {
    for (unsigned format=0; format<2; ++format) {
        struct syle_framebuffer fb=fixture(format);
        UINT32 fg=format?0xe1f0fa:0xfaf0e1, bg=format?0x08121e:0x1e1208;
        console_bind(&fb); assert(console_available()); check_snapshot(&fb);
        assert(screen.x==0 && screen.y==0 && screen.columns==3 && screen.rows==2);
        for (unsigned y=0; y<TEST_H; ++y)
            for (unsigned x=0; x<TEST_W; ++x)
                assert(pixels[1+y*TEST_STRIDE+x]==bg);
        console_putc('P'); check_cell(0,0,glyph_p,fg,bg);
        console_putc('a'); check_cell(1,0,glyph_a,fg,bg);
        console_putc((char)0xff); check_cell(2,0,glyph_question,fg,bg);
        assert(screen.x==3 && screen.y==0);
        console_putc('P'); assert(screen.x==1 && screen.y==1);
        check_cell(0,1,glyph_p,fg,bg); check_guards();
        console_panic_style();
        fg=format?0xffebe1:0xe1ebff; bg=format?0x58080c:0x0c0858;
        assert(screen.foreground==fg && screen.background==bg);
        assert(screen.x==0 && screen.y==0);
        for (unsigned y=0; y<TEST_H; ++y)
            for (unsigned x=0; x<TEST_W; ++x)
                assert(pixels[1+y*TEST_STRIDE+x]==bg);
        console_putc('P'); check_cell(0,0,glyph_p,fg,bg); check_guards();
    }
    struct syle_framebuffer fb=fixture(1);
    console_bind(&fb);
    /* Every byte classified as unsupported has the same exact '?' pixels. */
    for (unsigned c=0; c<256; ++c) {
        if ((c>=32 && c<=126) || c=='\r' || c=='\n' || c=='\t') continue;
        console_putc('\r'); console_putc((char)c);
        check_cell(0,0,glyph_question,0xe1f0fa,0x08121e);
    }
    /* All lower-case glyphs match the corresponding upper-case cell. */
    for (unsigned c='a'; c<='z'; ++c) {
        UINT32 lower[16*12];
        console_putc('\r'); console_putc((char)c);
        for (unsigned y=0; y<16; ++y)
            for (unsigned x=0; x<12; ++x) lower[y*12+x]=pixels[1+y*TEST_STRIDE+x];
        console_putc('\r'); console_putc((char)(c-'a'+'A'));
        for (unsigned y=0; y<16; ++y)
            for (unsigned x=0; x<12; ++x) assert(lower[y*12+x]==pixels[1+y*TEST_STRIDE+x]);
    }
    UINT32 before[TEST_N+2]; memcpy(before,pixels,sizeof pixels);
    console_putc('\r'); assert(screen.x==0 && screen.y==0);
    assert(memcmp(before,pixels,sizeof pixels)==0);
    /* TAB means exactly four spaces, including wrap into another row. */
    console_bind(&fb); console_putc('P'); console_putc('\t');
    unsigned tab_x=screen.x, tab_y=screen.y;
    memcpy(before,pixels,sizeof pixels);
    console_bind(&fb); console_putc('P');
    for (unsigned i=0; i<4; ++i) console_putc(' ');
    assert(screen.x==tab_x && screen.y==tab_y);
    assert(memcmp(before,pixels,sizeof pixels)==0);
    /* Distinct values make a wrong scroll source/extent observable. */
    for (unsigned y=0; y<TEST_H; ++y)
        for (unsigned x=0; x<TEST_W; ++x) pixels[1+y*TEST_STRIDE+x]=0x10000*y+x;
    memcpy(before,pixels,sizeof pixels);
    assert(screen.y==1); console_putc('\n');
    assert(screen.x==0 && screen.y==1);
    for (unsigned y=0; y<TEST_H; ++y)
        for (unsigned x=0; x<TEST_W; ++x) {
            UINT32 expected=y<16?before[1+(y+16)*TEST_STRIDE+x]:
                y<32?screen.background:before[1+y*TEST_STRIDE+x];
            assert(pixels[1+y*TEST_STRIDE+x]==expected);
        }
    check_guards();
}
static void validation_and_snapshot(void) {
    struct syle_framebuffer fb=fixture(0);
    for (unsigned which=0; which<15; ++which) {
        console_bind(&fb); assert(console_available());
        struct syle_framebuffer bad=fb, snap;
        switch (which) {
        case 0: bad.valid=0; break;
        case 1: bad.valid=2; break;
        case 2: bad.format=2; break;
        case 3: bad.width=11; break;
        case 4: bad.height=15; break;
        case 5: bad.width=8193; break;
        case 6: bad.height=8193; break;
        case 7: bad.stride=TEST_W-1; break;
        case 8: bad.stride=8193; break;
        case 9: bad.base=0; break;
        case 10: bad.base++; break;
        case 11: bad.size--; break;
        case 12: bad.base=UINT64_MAX-3; break;
        case 13: bad.base+=2; break;
        case 14: break; /* Null descriptor. */
        }
        UINT32 before[TEST_N+2]; memcpy(before,pixels,sizeof pixels);
        console_bind(which==14?NULL:&bad);
        assert(!console_available()); console_snapshot(&snap); assert(snap.valid==0);
        console_putc('X'); console_putc('\n'); console_putc('\t'); console_panic_style();
        assert(memcmp(before,pixels,sizeof pixels)==0); check_guards();
    }
    /* Accepted dimension endpoints use real, bounded host allocations. */
    const unsigned dims[][3]={{12,16,12},{8192,16,8192},{12,8192,12},{12,16,8192}};
    for (unsigned i=0; i<sizeof dims/sizeof dims[0]; ++i) {
        size_t count=(size_t)dims[i][1]*dims[i][2];
        UINT32 *buffer=malloc((count+2)*sizeof *buffer); assert(buffer);
        for (size_t j=0; j<count+2; ++j) buffer[j]=guard;
        fb=(struct syle_framebuffer){.base=(UINTN)(buffer+1),.size=count*4,
            .width=dims[i][0],.height=dims[i][1],.stride=dims[i][2],.valid=1};
        console_bind(&fb); assert(console_available()); check_snapshot(&fb);
        console_putc('P'); console_putc('\n');
        assert(buffer[0]==guard && buffer[count+1]==guard);
        for (unsigned y=0; y<fb.height; ++y)
            for (unsigned x=fb.width; x<fb.stride; ++x) assert(buffer[1+(size_t)y*fb.stride+x]==guard);
        console_bind(NULL); free(buffer);
    }
}
static EFI_STATUS locate_status;
static EFI_GRAPHICS_OUTPUT_PROTOCOL *located_gop;
static unsigned locate_calls;
static EFI_STATUS EFIAPI mock_locate(EFI_GUID *guid, void *registration, void **out) {
    const EFI_GUID expected=EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    assert(memcmp(guid,&expected,sizeof expected)==0 && registration==NULL);
    ++locate_calls; *out=located_gop; return locate_status;
}
static void firmware_adapter(void) {
    EFI_BOOT_SERVICES bs={0}; bs.LocateProtocol=mock_locate;
    EFI_GRAPHICS_OUTPUT_PROTOCOL gop={0};
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE mode={0};
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION info={0};
    for (unsigned which=0; which<10; ++which) {
        struct syle_framebuffer fb=fixture(0), snap;
        console_bind(&fb); assert(console_available());
        locate_status=EFI_SUCCESS; located_gop=&gop; locate_calls=0;
        gop.Mode=&mode; mode.Info=&info; mode.SizeOfInfo=sizeof info;
        mode.FrameBufferBase=fb.base; mode.FrameBufferSize=fb.size;
        info.HorizontalResolution=fb.width; info.VerticalResolution=fb.height;
        info.PixelsPerScanLine=fb.stride; info.PixelFormat=PixelRedGreenBlueReserved8BitPerColor;
        switch (which) {
        case 0: break; /* No Boot Services table. */
        case 1: locate_status=EFI_NOT_FOUND; break;
        case 2: located_gop=NULL; break;
        case 3: gop.Mode=NULL; break;
        case 4: mode.Info=NULL; break;
        case 5: mode.SizeOfInfo=sizeof info-1; break;
        case 6: info.PixelFormat=PixelBitMask; break;
        case 7: info.PixelFormat=PixelBltOnly; break;
        case 8: mode.FrameBufferSize--; break;
        case 9: info.PixelFormat=PixelFormatMax; break;
        }
        UINT32 before[TEST_N+2]; memcpy(before,pixels,sizeof pixels);
        console_init(which==0?NULL:&bs);
        assert(locate_calls==(which!=0)); assert(!console_available());
        console_snapshot(&snap); assert(snap.valid==0);
        console_putc('X'); assert(memcmp(before,pixels,sizeof pixels)==0);
    }
    for (unsigned format=0; format<2; ++format) {
        struct syle_framebuffer fb=fixture(format);
        locate_status=EFI_SUCCESS; located_gop=&gop; locate_calls=0;
        gop.Mode=&mode; mode.Info=&info; mode.SizeOfInfo=sizeof info;
        mode.FrameBufferBase=fb.base; mode.FrameBufferSize=fb.size;
        info.HorizontalResolution=fb.width; info.VerticalResolution=fb.height;
        info.PixelsPerScanLine=fb.stride;
        info.PixelFormat=format?PixelBlueGreenRedReserved8BitPerColor:PixelRedGreenBlueReserved8BitPerColor;
        console_init(&bs); assert(console_available() && locate_calls==1); check_snapshot(&fb);
        /* Later output must use copied metadata, not the mutable GOP objects. */
        mode.FrameBufferBase=0; mode.Info=NULL; gop.Mode=NULL; located_gop=NULL;
        info.HorizontalResolution=0;
        console_putc('P'); check_cell(0,0,glyph_p,format?0xe1f0fa:0xfaf0e1,format?0x08121e:0x1e1208);
        check_snapshot(&fb); assert(locate_calls==1); check_guards();
    }
}
int main(void) {
    legacy_guard_test(); pixels_and_controls(); validation_and_snapshot(); firmware_adapter();
    puts("PASS CONSOLE exact pixels, RGB/BGR, all unsupported bytes, case folding, controls, scroll, snapshot, metadata bounds and mocked GOP adapter");
}
