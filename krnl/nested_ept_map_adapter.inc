            if(!composed_ready||composed.poisoned||l2_exits>=4||rsp!=0x9000)break;
            uint64_t q=0,gpa=0,gla=0;
            if(l2_exits==0||l2_exits==3) {
                if(reason!=18||rip!=label(l2_exits?nested_map_done_call:nested_map_warm_call)||
                   !get(0x440c,&length)||length!=3||*(uint64_t *)ram(f,0x8000)!=0x77||
                   *(uint64_t *)ram(f,0x9000)!=0x88)break;
                if(l2_exits==3&&(vmx_guest_regs.rax!=0x99||*(uint64_t *)ram(f,0xa000)!=0x99||
                   invept_count!=2||composed.generation!=3))break;
                log_text(l2_exits?"NESTED.MAP_REMAP_NEW_BACKING_READ_99\n":"NESTED.MAP_TWO_OLD_MAPPINGS_WARMED\n");
                if(!nvmx_entry_commit(&model)||!nvmx_exit_store(&model,0x440c,length))break;
            } else {
                uint8_t access=l2_exits==1?2:1;
                if(reason!=48||invept_count!=1||composed.generation!=2||
                   rip!=label(l2_exits==1?nested_map_write_fault:nested_map_read_fault)||
                   !get(0x6400,&q)||!get(0x2400,&gpa)||!get(0x640a,&gla)||
                   gpa!=(l2_exits==1?0x14000u:0x15000u)||gla!=gpa||(q&0x187)!=(0x180u|access))break;
                /* Classify only the last successfully published cache. L1
                 * may have changed EPT12 without INVEPT; don't read it here. */
                struct nested_ept_result cached=nested_ept_map_classify(&composed.committed,gpa,access);
                if(cached.status!=NESTED_EPT_L1_DENIED)break; /* L0 failure is not L1's violation */
                if(cached.effective12!=(l2_exits==1?1u:0u))break;
                q=(q&UINT64_C(0x187))|((uint64_t)cached.effective12<<3);
                if(!nvmx_entry_commit(&model)||!nvmx_exit_store(&model,0x6400,q)||
                   !nvmx_exit_store(&model,0x2400,gpa)||!nvmx_exit_store(&model,0x640a,gla))break;
                log_text(l2_exits==1?"NESTED.MAP_REVOKED_WRITE_REFLECTED\n":"NESTED.MAP_UNMAPPED_READ_REFLECTED\n");
            }
            if(!nvmx_exit_store(&model,0x681e,rip)||!nvmx_exit_store(&model,0x681c,rsp)||
               !nvmx_exit_store(&model,0x6820,flags)||!nvmx_exit_store(&model,0x4402,reason)||
               !nvmx_exit_store(&model,0x4404,0)||!nvmx_return_from_l2(&model)||
               !load(vmcs01)||!reflect_host(&profile))break;
