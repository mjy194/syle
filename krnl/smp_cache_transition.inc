/* BSP only, after actual exits and closed shootdown entry gate. No target
 * memory dereference occurs while its host PTE is absent. */
static void mp_transition_run(volatile uint32_t *lapic,uint32_t ap,bool to_uc){
    uint64_t epoch;
    if(!mp_transition.generation){
        controller_require(cache_mp_transition_init(&mp_transition,3));
        controller_require(cache_maintenance_lease(&bsp_cache,&mp_host_lease));
    }
    controller_require(controller.running==0&&controller.phase==SHOOTDOWN_PUBLISHING);
    controller_require(cache_mp_transition_begin(&mp_transition,mp_transition.generation,
        to_uc?CACHE_MP_OPEN_UC:CACHE_MP_OPEN_WB,true,true,&epoch));
    struct cache_ept_alias_view v={.eptp=shared_ept.eptp};
    for(unsigned i=0;i<4;i++){v.hpa[i]=shared_ept.table[i];v.table[i]=paging_phys_to_virt(v.hpa[i]);}
    controller_require(cache_ept_alias_prepare(&mp_alias,&v,shared_ept.page[7],to_uc));
    log_text("SMP.MP_TARGET_EPT_ALIAS_COUNT=");log_hex(mp_alias.count);
    uint64_t *pte=&((uint64_t *)paging_phys_to_virt(mp_host_lease.table_hpa))[mp_host_lease.index];
    for(unsigned n=0;n<6;n++){
        enum cache_mp_phase phase=mp_transition.phase;
        controller_require(!cache_mp_transition_entry_allowed(&mp_transition,epoch));
        if(phase==CACHE_MP_DETACH_EPT){
            controller_require(cache_ept_alias_detach(&mp_alias));
            controller_require(cache_mp_transition_publish(&mp_transition,epoch,phase));
        }else if(phase==CACHE_MP_BREAK_TLB){
            uint64_t expected=(mp_host_lease.oldraw&~UINT64_C(0x98))|(to_uc?0:24);
            controller_require(((*pte^expected)&~UINT64_C(0x60))==0);
            __atomic_store_n(pte,0,__ATOMIC_RELEASE);
            controller_require(cache_mp_transition_publish(&mp_transition,epoch,phase));
        }else if(phase==CACHE_MP_MAKE_TLB){
            controller_require(*pte==0);
            uint64_t next=(mp_host_lease.oldraw&~UINT64_C(0x98))|(to_uc?24:0);
            __atomic_store_n(pte,next,__ATOMIC_RELEASE);
            controller_require(cache_mp_transition_publish(&mp_transition,epoch,phase));
        }else if(phase==CACHE_MP_PUBLISH_EPT){
            controller_require(cache_ept_alias_publish(&mp_alias));
            controller_require(cache_mp_transition_publish(&mp_transition,epoch,phase));
        }
        bool local=false;
        if(phase==CACHE_MP_DETACH_EPT||phase==CACHE_MP_PUBLISH_EPT)local=smp_guest_local_invalidate(&bsp_guest);
        else if(phase==CACHE_MP_BREAK_TLB||phase==CACHE_MP_MAKE_TLB)local=cache_maintenance_tlb(&bsp_cache);
        else if(phase==CACHE_MP_BREAK_CACHE||phase==CACHE_MP_MAKE_CACHE)local=cache_maintenance_cache(&bsp_cache);
        controller_require(local);
        remote_command(lapic,ap,OP_MP_TRANSITION,epoch,phase);
        controller_require(box.completed_phase==(uint64_t)phase);
        controller_require(cache_mp_transition_ack(&mp_transition,0,epoch,phase)&&cache_mp_transition_ack(&mp_transition,1,epoch,phase));
        log_text("SMP.MP_TYPE_PHASE_ACK CPUs=2 epoch=");log_hex(epoch);log_text("SMP.MP_TYPE_PHASE=");log_hex(phase);
        controller_require(cache_mp_transition_advance(&mp_transition,epoch));
    }
    controller_require(cache_mp_transition_finish(&mp_transition,epoch));
    if(!to_uc)controller_require(cache_page_lease_matches(&mp_host_lease,*pte));
    log_text(to_uc?"SMP.MP_UC_PUBLISHED host_and_EPT=UC all_ACK=1\n":"SMP.MP_WB_RESTORED host_and_EPT=WB all_ACK=1\n");
}
