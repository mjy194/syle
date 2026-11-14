#include "pci_probe_nodma.h"
static bool refuse_admit(void*x,uint64_t a,size_t n,bool w){(void)x;(void)a;(void)n;(void)w;return false;}
static bool refuse_read(void*x,uint64_t a,void*b,size_t n){(void)x;(void)a;(void)b;(void)n;return false;}
static bool refuse_write(void*x,uint64_t a,const void*b,size_t n){(void)x;(void)a;(void)b;(void)n;return false;}
/* Unreachable through supported request path: PFN admission/read always fails.
 * Not a publication primitive and MUST NOT be reused with real DMA callbacks. */
static void no_publication(void*x){(void)x;}
bool vfb_init_nodma_probe(struct vfb*s,uint64_t epoch,uint64_t ref,uint64_t gap){
 static const uint8_t placeholder[512]={0};
 struct vro_io io={0,refuse_admit,refuse_read,refuse_write,no_publication};
 return vfb_init_projected(s,io,placeholder,sizeof placeholder,epoch,ref,gap);
}
