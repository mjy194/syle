#ifndef LINUX_BOOT_H
#define LINUX_BOOT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define LB_RAM_SIZE (128ull*1024*1024)
struct lb_writer { void *context; bool (*write)(void*,uint64_t,const void*,size_t); bool (*zero)(void*,uint64_t,size_t); };
struct lb_plan { uint64_t ram_size,kernel_addr,kernel_size,kernel_envelope,initrd_addr,initrd_size,params_addr,cmdline_addr,entry_addr,payload_offset,cmdline_size; };
struct lb_entry_state { uint64_t rip,rsi,rsp,cr0,cr3,cr4,efer,gdt_base; uint16_t gdt_limit,cs,ds; };
/* Reconstructed direct64 boot design, not byte-original recovery. Sources and
 * writer must remain immutable and disjoint from destination RAM throughout.
 * Failure may partially write RAM: caller owns cleanup. No firmware/VM entry. */
bool lb_inspect(const void*,size_t,size_t,const char*,struct lb_plan*);
bool lb_load(const void*,size_t,const void*,size_t,const char*,const struct lb_writer*,struct lb_plan*);
bool lb_entry_prepare(const struct lb_plan*,const struct lb_writer*,struct lb_entry_state*);
#endif
