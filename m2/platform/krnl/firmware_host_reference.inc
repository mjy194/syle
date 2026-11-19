/* The outer hypervisor supplies one reference page for the Syle VM.
 * Only the BSP samples it; AP VMCS operations go through the mailbox. */
struct m2_hv_tsc_page {
 uint32_t sequence,reserved;
 uint64_t scale,offset;
};
static struct {
 struct lhcr_source fallback;
 const struct m2_hv_tsc_page *view;
 uint64_t page,last,fast_reads;
 uint32_t verified_sequence;
} m2_hv_ref;
extern bool lvn_read_reference_tsc_page_msr(uint64_t*);
extern bool lvn_write_reference_tsc_page_msr(uint64_t);

static uint32_t m2_hv_ref_read(uint64_t *value){
 const struct m2_hv_tsc_page *p=m2_hv_ref.view;
 uint32_t seq=__atomic_load_n(&p->sequence,__ATOMIC_ACQUIRE);
 if(!seq)return 0;
 uint64_t scale=__atomic_load_n(&p->scale,__ATOMIC_RELAXED);
 uint64_t offset=__atomic_load_n(&p->offset,__ATOMIC_RELAXED);
 uint32_t a,d;
 __asm__ volatile("lfence;rdtsc;lfence":"=a"(a),"=d"(d)::"memory");
 if(!scale||seq!=__atomic_load_n(&p->sequence,__ATOMIC_ACQUIRE))return 0;
 uint64_t tsc=((uint64_t)d<<32)|a;
 *value=(uint64_t)(((__uint128_t)tsc*scale)>>64)+offset;
 return seq;
}

static bool m2_hv_ref_sample(void *arg,struct lhc_sample *sample){
 (void)arg;
 const struct lhcr_source *slow=&m2_hv_ref.fallback;
 uint64_t value;
 uint32_t seq=m2_hv_ref_read(&value);
 if(seq&&value>=m2_hv_ref.last){
  if(m2_hv_ref.verified_sequence!=seq){
   struct lhc_sample before,after;
   if(!slow->sample(slow->ctx,&before))return false;
   seq=m2_hv_ref_read(&value);
   if(!slow->sample(slow->ctx,&after))return false;
   if(!seq||value<before.tsc||value>after.tsc||value<m2_hv_ref.last){
    m2_hv_ref.verified_sequence=0;
    if(after.tsc<m2_hv_ref.last)return false;
    *sample=after;m2_hv_ref.last=after.tsc;return true;
   }
   m2_hv_ref.verified_sequence=seq;
  }
  *sample=(struct lhc_sample){value,slow->properties.epoch,slow->properties.cpu};
  m2_hv_ref.last=value;m2_hv_ref.fast_reads++;
  return true;
 }
 /* Sequence zero is expected before the outer hypervisor publishes the
  * page. It can also invalidate the conversion while updating the clock. */
 m2_hv_ref.verified_sequence=0;
 if(!slow->sample(slow->ctx,sample)||sample->tsc<m2_hv_ref.last)return false;
 m2_hv_ref.last=sample->tsc;
 return true;
}

static bool m2_hv_ref_start(struct lhcr_source *source,uint64_t last){
 uint32_t a,b,c,d;
 __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d)
                  :"a"(0x40000003),"c"(0):"memory");
 uint64_t previous;
 if(!(a&(1u<<9))||!lvn_read_reference_tsc_page_msr(&previous)||previous)return true;
 uint64_t page=pmm_alloc_page();
 if(!page)return true;
 uint8_t *view=paging_phys_to_virt(page);
 if(!view)return pmm_free_page(page)!=0;
 for(unsigned i=0;i<4096;i++)view[i]=0;
 if(!lvn_write_reference_tsc_page_msr(page|1u))return pmm_free_page(page)!=0;
 m2_hv_ref.fallback=*source;
 m2_hv_ref.view=(const struct m2_hv_tsc_page*)view;
 m2_hv_ref.page=page;m2_hv_ref.last=last;
 source->sample=m2_hv_ref_sample;
 return true;
}

static bool m2_hv_ref_stop(void){
 if(!m2_hv_ref.page)return true;
 if(!lvn_write_reference_tsc_page_msr(0)||!pmm_free_page(m2_hv_ref.page))return false;
 m2_hv_ref=(__typeof__(m2_hv_ref)){0};
 return true;
}
