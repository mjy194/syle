            if(!composed_ready||composed.tree.poisoned||l2_exits>=4||rsp!=0x9000||
               (reason&(UINT64_C(1)<<31)))break;
            if(model.prepared){if(!nvmx_entry_commit(&model))break;}
            else if(!model.running)break;
            uint64_t q=0,gpa=0,gla=0;
            if(reason==48) {
                if(!get(0x6400,&q)||!get(0x2400,&gpa))break;
                uint8_t need=q&7;
                if(!need)break;
                struct nested_ept_result cached;
                enum nested_sparse_fault action=nested_sparse_hw_tree_fault_policy(&composed,&composed.tree,0x1001e,gpa,need,&cached,&large_policy);
                if(action==NS_FAULT_FILLED) {
                    /* Includes guest paging-structure misses. GPA, never an
                     * undefined GLA, identifies the needed translation. */
                    sparse_fills++;
                    if(gpa==0x208000)high_seen|=1;
                    if(gpa==0x40009000)high_seen|=2;
                    log_text("NESTED.LARGE_L0_FILL_GPA=");log_hex(gpa);
                    continue; /* same RIP, already running, physical VMRESUME */
                }
                if(action!=NS_FAULT_L1)break; /* backing/malformed/cachedOK are not L1 exits */
                /* Only direct final accesses are reflected in this profile;
                 * unsupported guest-walk/indirect-delivery faults fail closed. */
                if((q&0x180)!=0x180||!get(0x640a,&gla)||invept_count!=1||
                   composed.tree.generation!=2||(l2_exits!=1&&l2_exits!=2)||
                   need!=(l2_exits==1?2u:1u)||
                   rip!=label(l2_exits==1?nested_large_write_fault:nested_large_read_fault)||
                   gpa!=(l2_exits==1?UINT64_C(0x208000):UINT64_C(0x40009000))||
                   gla!=(l2_exits==1?0x30000u:0x31000u)||
                   cached.effective12!=(l2_exits==1?1u:0u))break;
                q=(q&UINT64_C(0x187))|((uint64_t)cached.effective12<<3);
                if(!nvmx_exit_store(&model,0x6400,q)||!nvmx_exit_store(&model,0x2400,gpa)||
                   !nvmx_exit_store(&model,0x640a,gla))break;
                log_text(l2_exits==1?"NESTED.LARGE_REVOKED_WRITE_REFLECTED\n":"NESTED.LARGE_HIGH_UNMAP_REFLECTED\n");
            } else if(reason==18) {
                if((l2_exits!=0&&l2_exits!=3)||
                   rip!=label(l2_exits?nested_large_done_call:nested_large_warm_call)||
                   !get(0x440c,&length)||length!=3||high_seen!=3||
                   *(uint64_t *)ram(f,0x8000)!=0x11||*(uint64_t *)ram(f,0x9000)!=0x22)break;
                if(l2_exits==3&&(vmx_guest_regs.rax!=0x22||invept_count!=2||composed.tree.generation!=3))break;
                if(!nvmx_exit_store(&model,0x440c,length))break;
                log_text(l2_exits?"NESTED.LARGE_RESTORED_1G_READ_22\n":"NESTED.LARGE_TWO_LEAF_SIZES_WARMED\n");
            } else break;
            if(!nvmx_exit_store(&model,0x681e,rip)||!nvmx_exit_store(&model,0x681c,rsp)||
               !nvmx_exit_store(&model,0x6820,flags)||!nvmx_exit_store(&model,0x4402,reason)||
               !nvmx_exit_store(&model,0x4404,0)||!nvmx_return_from_l2(&model)||
               !load(vmcs01)||!reflect_host(&profile))break;
