            if(!composed_ready||contexts.poisoned||l2_exits>=8||rsp!=0x9000||
               (reason&(UINT64_C(1)<<31)))break;
            if(model.prepared){if(!nvmx_entry_commit(&model))break;}
            else if(!model.running)break;
            if(reason==48) {
                uint64_t q,gpa;
                if(!get(0x6400,&q)||!get(0x2400,&gpa)||!(q&7))break;
                struct nested_ept_sparse *active=nested_ept_contexts_active(&contexts);
                struct nested_ept_result cached;
                if(!active||nested_sparse_hw_tree_fault(&composed,active,selected12,gpa,q&7,&cached)!=NS_FAULT_FILLED)break;
                sparse_fills++;continue; /* internal miss is not another virtual entry */
            }
            static const uint64_t values[]={0x11,0x22,0x11,0x22,0x33,0x22,0x44,0x33};
            static const uint64_t keys[]={0x1001e,0x1401e,0x1001e,0x1401e,0x1001e,0x1401e,0x1401e,0x1001e};
            if(reason!=18||rip!=label(nested_context_call)||!get(0x440c,&length)||length!=3||
               vmx_guest_regs.rax!=values[l2_exits]||selected12!=keys[l2_exits])break;
            log_text("NESTED.CONTEXT_CHECKPOINT=");log_hex(l2_exits);
            log_text("NESTED.CONTEXT_EPTP12=");log_hex(selected12);
            log_text("NESTED.CONTEXT_ACTUAL_VALUE=");log_hex(vmx_guest_regs.rax);
            if(!nvmx_exit_store(&model,0x681e,rip)||!nvmx_exit_store(&model,0x681c,rsp)||
               !nvmx_exit_store(&model,0x6820,flags)||!nvmx_exit_store(&model,0x4402,reason)||
               !nvmx_exit_store(&model,0x440c,length)||!nvmx_exit_store(&model,0x4404,0)||
               !nvmx_return_from_l2(&model)||!load(vmcs01)||!reflect_host(&profile))break;
