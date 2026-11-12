            if(!composed_ready||composed.tree.poisoned||l2_exits>=2||rsp!=0x9000||
               (reason&(UINT64_C(1)<<31)))break;
            if(model.prepared){if(!nvmx_entry_commit(&model))break;}else if(!model.running)break;
            uint64_t actual_pat,actual_cr0;
            if(!get(0x2804,&actual_pat)||actual_pat!=UINT64_C(0x0606060606060006)||
               !get(0x6800,&actual_cr0)||(actual_cr0&UINT64_C(0x60000000)))break;
            if(reason==48){
                uint64_t q,gpa;struct nested_ept_result r;
                if(!get(0x6400,&q)||!get(0x2400,&gpa)||!(q&7))break;
                if(nested_ept_sparse_lookup(&composed.tree,gpa,q&7,&r)!=NESTED_SPARSE_MISS)break;
                const struct nested_ept_policy policy={profile.physical_bits,false,false};
                r=nested_ept_walk_memory(0x1001e,gpa&~UINT64_C(4095),1,cache_registered_resolve,f,&policy);
                r=nested_sparse_walk_access(r,q&7);
                if(!cache_registered_admit(f,&r)||r.status!=NESTED_EPT_OK||
                   (r.leaf_type!=6&&((q&0x180)!=0x180||(q&4)))||
                   nested_ept_sparse_fill(&composed.tree,gpa&~UINT64_C(4095),&r)!=NESTED_SPARSE_OK)break;
#ifdef VMX_NESTED_WT_TEST
                if(r.leaf_type==4){log_text("NESTED.WT_EPT02_LEAF type4_IPAT1=1 GPA=");log_hex(gpa);}
#endif
                sparse_fills++;continue;
            }
            if(reason!=18||rip!=label(l2_exits?nested_uc_done_call:nested_uc_warm_call)||
               !get(0x440c,&length)||length!=3||vmx_guest_regs.rax!=(l2_exits?0x99u:0x77u)||
               *(volatile uint64_t *)ram(f,0xf000)!=vmx_guest_regs.rax||invept_count!=l2_exits)break;
#ifdef VMX_NESTED_WT_TEST
            log_text("NESTED.REGISTERED_WT_READ=");
#else
            log_text("NESTED.REGISTERED_UC_READ=");
#endif
            log_hex(vmx_guest_regs.rax);
            if(!nvmx_exit_store(&model,0x2804,actual_pat)||!nvmx_exit_store(&model,0x681e,rip)||
               !nvmx_exit_store(&model,0x681c,rsp)||!nvmx_exit_store(&model,0x6820,flags)||
               !nvmx_exit_store(&model,0x4402,reason)||!nvmx_exit_store(&model,0x440c,length)||
               !nvmx_exit_store(&model,0x4404,0)||!nvmx_return_from_l2(&model)||!load(vmcs01)||!reflect_host(&profile))break;
