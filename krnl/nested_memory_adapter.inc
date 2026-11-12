            if(!composed_ready||composed.tree.poisoned||l2_exits||rsp!=0x9000||
               (reason&(UINT64_C(1)<<31)))break;
            if(model.prepared){if(!nvmx_entry_commit(&model))break;}
            else if(!model.running)break;
            uint64_t actual_pat,actual_cr0;
            if(!get(0x2804,&actual_pat)||actual_pat!=UINT64_C(0x0606060606060006)||
               !get(0x6800,&actual_cr0)||(actual_cr0&UINT64_C(0x60000000)))break;
            if(reason==48) {
                uint64_t q,gpa;struct nested_ept_result r;
                if(!get(0x6400,&q)||!get(0x2400,&gpa)||!(q&7))break;
                if(nested_ept_sparse_lookup(&composed.tree,gpa,q&7,&r)!=NESTED_SPARSE_MISS)break;
                const struct nested_ept_policy policy={profile.physical_bits,true,true};
                r=nested_ept_walk_memory(0x1001e,gpa&~UINT64_C(4095),1,
                    nested_sparse_hw_resolve,f,&policy);
                r=nested_sparse_walk_access(r,q&7);
                /* All mappings, not just the demonstrated data alias, must
                 * ignore PAT. No single GVA observation can classify a GPA. */
                if(r.status!=NESTED_EPT_OK||r.walk_type!=6||!r.ignore_pat||
                   !nested_memory_wb_leaf_safe(r.leaf_type,r.ignore_pat,false,actual_pat)||
                   nested_ept_sparse_fill(&composed.tree,gpa&~UINT64_C(4095),&r)!=NESTED_SPARSE_OK)break;
                sparse_fills++;continue;
            }
            if(reason!=18||rip!=label(nested_memory_call)||!get(0x440c,&length)||length!=3||
               vmx_guest_regs.rax!=0x99||*(uint64_t *)ram(f,0x8000)!=0x99)break;
            log_text("NESTED.MEMORY_ACTUAL_GUEST_PAT=");log_hex(actual_pat);
            log_text("NESTED.MEMORY_SAME_GPA_PAT0_WB_PAT1_UC_IPAT_READ_WRITE_99\n");
            if(!nvmx_exit_store(&model,0x2804,actual_pat)||
               !nvmx_exit_store(&model,0x681e,rip)||!nvmx_exit_store(&model,0x681c,rsp)||
               !nvmx_exit_store(&model,0x6820,flags)||!nvmx_exit_store(&model,0x4402,reason)||
               !nvmx_exit_store(&model,0x440c,length)||!nvmx_exit_store(&model,0x4404,0)||
               !nvmx_return_from_l2(&model)||!load(vmcs01)||!reflect_host(&profile))break;
