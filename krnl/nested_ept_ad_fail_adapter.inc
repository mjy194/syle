            if(!composed_ready||composed.tree.poisoned||l2_exits>=4||rsp!=0x9000||
               (reason&(UINT64_C(1)<<31)))break;
            if(model.prepared){if(!nvmx_entry_commit(&model))break;}
            else if(!model.running)break;
            if(reason==48) {
                uint64_t q,gpa,gla;struct nested_ept_result result;
                if(!get(0x6400,&q)||!get(0x2400,&gpa)||!(q&7))break;
                enum nested_ad_action action=nested_ept_ad_hw_handle(&ad_cache,&composed,gpa,q&7,&result);
                if(action==NESTED_AD_RETRY){sparse_fills++;continue;}
                if(action!=NESTED_AD_L1_DENIED||(q&0x180)!=0x180||!get(0x640a,&gla)||
                   gpa!=0xf000||gla!=0xf000||(l2_exits!=0&&l2_exits!=2)||
                   (q&7)!=(l2_exits?2u:1u)||result.effective12!=(l2_exits?1u:0u)||
                   rip!=label(l2_exits?nested_ad_fail_write:nested_ad_fail_read))break;
                uint64_t terminal=*(uint64_t *)ram(f,0x13078);
                if(terminal!=(l2_exits?0x8131u:0xdead000u)||
                   !(*(uint64_t *)ram(f,0x10000)&0x100)||!(*(uint64_t *)ram(f,0x11000)&0x100)||
                   !(*(uint64_t *)ram(f,0x12000)&0x100))break;
                q=(q&UINT64_C(0x187))|((uint64_t)result.effective12<<3);
                if(!nvmx_exit_store(&model,0x6400,q)||!nvmx_exit_store(&model,0x2400,gpa)||
                   !nvmx_exit_store(&model,0x640a,gla))break;
                log_text(l2_exits?"NESTED.AD_RO_DENIED_A1_D0\n":"NESTED.AD_NONPRESENT_PREFIX_A_TERMINAL_UNCHANGED\n");
            } else {
                if(reason!=18||(l2_exits!=1&&l2_exits!=3)||
                   rip!=label(l2_exits==1?nested_ad_fail_read_done:nested_ad_fail_done)||
                   !get(0x440c,&length)||length!=3)break;
                if(!nvmx_exit_store(&model,0x440c,length))break;
                log_text("NESTED.AD_FAILURE_REPAIRED_CHECKPOINT=");log_hex(l2_exits);
            }
            if(!nvmx_exit_store(&model,0x681e,rip)||!nvmx_exit_store(&model,0x681c,rsp)||
               !nvmx_exit_store(&model,0x6820,flags)||!nvmx_exit_store(&model,0x4402,reason)||
               !nvmx_exit_store(&model,0x4404,0)||!nvmx_return_from_l2(&model)||
               !load(vmcs01)||!reflect_host(&profile))break;
