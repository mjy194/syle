#include "linux_vcpu_host.h"
#include <stddef.h>
#include <stdio.h>
typedef bool (*lgm_sig)(struct linux_guest_memory *,const struct sg_memory_ops *,const void *,size_t,const void *,size_t,const char *);
typedef bool (*lvh_sig)(struct lvh_context *,const struct lvh_ops *,const struct linux_guest_memory *,uint64_t,uint64_t);
typedef bool (*lgm_free_sig)(struct linux_guest_memory *);
typedef bool (*lvh_free_sig)(struct lvh_context *);
_Static_assert(_Generic(&lgm_prepare,lgm_sig:1,default:0),"legacy lgm_prepare signature");
_Static_assert(_Generic(&lvh_open,lvh_sig:1,default:0),"legacy lvh_open signature");
_Static_assert(_Generic(&lgm_release,lgm_free_sig:1,default:0),"legacy lgm_release signature");
_Static_assert(_Generic(&lvh_close,lvh_free_sig:1,default:0),"legacy lvh_close signature");
#define S(t) printf(#t ".size=%zu align=%zu\n",sizeof(struct t),_Alignof(struct t))
#define O(t,f) printf(#t "." #f "=%zu\n",offsetof(struct t,f))
int main(void){
 S(sg_memory_ops);O(sg_memory_ops,context);O(sg_memory_ops,allocate);O(sg_memory_ops,release);O(sg_memory_ops,map);O(sg_memory_ops,writeback);
 S(linux_guest_memory);O(linux_guest_memory,owner);O(linux_guest_memory,pages);O(linux_guest_memory,tables);O(linux_guest_memory,eptp);O(linux_guest_memory,plan);O(linux_guest_memory,entry);O(linux_guest_memory,prepared);
 S(lvh_ops);O(lvh_ops,context);O(lvh_ops,memory);O(lvh_ops,physical_bits);O(lvh_ops,intel_vmx);O(lvh_ops,capture);O(lvh_ops,read_msr);O(lvh_ops,vmxon);O(lvh_ops,vmclear);O(lvh_ops,vmptrld);O(lvh_ops,vmwrite);O(lvh_ops,invept);O(lvh_ops,vmxoff);
 S(lvh_context);O(lvh_context,owner);O(lvh_context,host);O(lvh_context,caps);O(lvh_context,plan);O(lvh_context,basic);O(lvh_context,vmxon_page);O(lvh_context,vmcs_page);O(lvh_context,eptp);O(lvh_context,active);O(lvh_context,ready);O(lvh_context,quarantined);O(lvh_context,open_stage);return 0;
}
