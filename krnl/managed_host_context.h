#ifndef SYLE_MANAGED_HOST_CONTEXT_H
#define SYLE_MANAGED_HOST_CONTEXT_H
#include <stdint.h>
#include <stdbool.h>
struct mhc_selectors {uint16_t ds,es,ss;};
struct mhc_saved {uint64_t caller_root,kernel_root;struct mhc_selectors selectors;bool entered;};
/* Trusted BSP kernel callbacks only. Writes either complete or fatal-stop.
 * Both roots map the supervisor callback code, stack, and saved object identically.
 * No user pointers may be accessed while the kernel root is selected. */
struct mhc_ops {void*ctx;uint64_t(*root)(void*);void(*set_root)(void*,uint64_t);struct mhc_selectors(*selectors)(void*);void(*set_selectors)(void*,struct mhc_selectors);};
static inline bool mhc_enter(struct mhc_saved*s,const struct mhc_ops*o,uint64_t kernel){
 if(!s||!o||!o->root||!o->set_root||!o->selectors||!o->set_selectors||s->entered||!kernel||(kernel&4095))return false;
 uint64_t current=o->root(o->ctx);struct mhc_selectors seg=o->selectors(o->ctx);
 if(!current||(current&4095)||(seg.ss!=0&&seg.ss!=0x10))return false;
 s->caller_root=current;s->kernel_root=kernel;s->selectors=seg;
 if(current!=kernel)o->set_root(o->ctx,kernel);
 o->set_selectors(o->ctx,(struct mhc_selectors){0x10,0x10,0x10});s->entered=true;return true;
}
static inline bool mhc_leave(struct mhc_saved*s,const struct mhc_ops*o){
 if(!s||!o||!s->entered||o->root(o->ctx)!=s->kernel_root)return false;
 struct mhc_selectors seg=o->selectors(o->ctx);
 if(seg.ds!=0x10||seg.es!=0x10||seg.ss!=0x10)return false;
 o->set_selectors(o->ctx,s->selectors);
 if(s->caller_root!=s->kernel_root)o->set_root(o->ctx,s->caller_root);
 s->entered=false;return true;
}
#endif
