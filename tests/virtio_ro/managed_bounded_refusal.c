#include "managed_bounded_candidate.h"
/* Native static-link profile ONLY. Not the host implementation. These fixed
 * refusals cannot be enabled by a caller boolean or a source/marker claim.
 * No address translation, target memory access or VMX instruction is issued. */
bool cb_page_zero(void*c,uint64_t p,enum sg_memory_role r,size_t o,size_t n){(void)c;(void)p;(void)r;(void)o;(void)n;return false;}
bool cb_page_write(void*c,uint64_t p,enum sg_memory_role r,size_t o,enum cs_source s,const void*b,size_t n){(void)c;(void)p;(void)r;(void)o;(void)s;(void)b;(void)n;return false;}
bool cb_page_u64(void*c,uint64_t p,enum sg_memory_role r,size_t o,uint64_t v){(void)c;(void)p;(void)r;(void)o;(void)v;return false;}
bool mbp_host_open(struct lvh_context*h,const struct lvh_ops*o,const struct linux_guest_memory*m,uint64_t s,uint64_t p,sg_allocate_role_fn f){(void)h;(void)o;(void)m;(void)s;(void)p;(void)f;return false;}
bool mbp_prepare_done(void*c){(void)c;return false;}
