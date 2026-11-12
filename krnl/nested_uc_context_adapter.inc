            struct nested_ept_sparse *uc_tree=nested_ept_contexts_active(&contexts);
            if(!composed_ready||!uc_tree||contexts.poisoned||uc_tree->poisoned||l2_exits>=7||rsp!=0x9000||
               (reason&(UINT64_C(1)<<31)))break;
            if(model.prepared){if(!nvmx_entry_commit(&model))break;}else if(!model.running)break;
            uint64_t actual_pat,actual_cr0;
            if(!get(0x2804,&actual_pat)||actual_pat!=UINT64_C(0x0606060606060006)||
               !get(0x6800,&actual_cr0)||(actual_cr0&UINT64_C(0x60000000)))break;
            if(reason==48){
                uint64_t q,gpa;struct nested_ept_result r;
                if(!get(0x6400,&q)||!get(0x2400,&gpa)||!(q&7))break;
                if(nested_ept_sparse_lookup(uc_tree,gpa,q&7,&r)!=NESTED_SPARSE_MISS)break;
                const struct nested_ept_policy policy={profile.physical_bits,false,false};
                r=nested_ept_walk_memory(selected12,gpa&~UINT64_C(4095),1,cache_registered_resolve,f,&policy);
                r=nested_sparse_walk_access(r,q&7);
                if(!cache_registered_admit(f,&r)||r.status!=NESTED_EPT_OK||
                   (r.leaf_type==0&&((q&0x180)!=0x180||(q&4)))||
                   nested_ept_sparse_fill(uc_tree,gpa&~UINT64_C(4095),&r)!=NESTED_SPARSE_OK)break;
                sparse_fills++;continue;
            }
            if(reason!=18||rip!=label(nested_uc_context_call)||
               !get(0x440c,&length)||length!=3||vmx_guest_regs.rax!=(0x77u+l2_exits)||
               *(volatile uint64_t *)ram(f,0xf000)!=vmx_guest_regs.rax||invept_count!=(l2_exits<5?0u:l2_exits-4))break;
            log_text("NESTED.CONTEXT_UC_READ=");log_hex(vmx_guest_regs.rax);
            if(!nvmx_exit_store(&model,0x2804,actual_pat)||!nvmx_exit_store(&model,0x681e,rip)||
               !nvmx_exit_store(&model,0x681c,rsp)||!nvmx_exit_store(&model,0x6820,flags)||
               !nvmx_exit_store(&model,0x4402,reason)||!nvmx_exit_store(&model,0x440c,length)||
               !nvmx_exit_store(&model,0x4404,0)||!nvmx_return_from_l2(&model)||!load(vmcs01)||!reflect_host(&profile))break;
