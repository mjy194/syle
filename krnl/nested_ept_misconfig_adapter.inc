            if(!composed_ready||composed.tree.poisoned||l2_exits>=4||rsp!=0x9000||
               (reason&(UINT64_C(1)<<31)))break;
            if(model.prepared){if(!nvmx_entry_commit(&model))break;}
            else if(!model.running)break;
            if(reason==48) {
                uint64_t q,gpa;
                if(!get(0x6400,&q)||!get(0x2400,&gpa)||!(q&7))break;
                struct nested_ept_result result;
                enum nested_sparse_fault action=nested_sparse_hw_classified_fault(&composed,gpa,q&7,&result,&large_policy);
                if(action==NS_FAULT_FILLED){sparse_fills++;continue;}
                /* Only an uncached EPT12 malformed translation can become 49.
                 * Physical EPT02 reason49, L0 ownership errors and unsupported
                 * features never reach this branch as a virtual misconfig. */
                if(action!=NS_FAULT_MISCONFIG||(q&0x180)!=0x180||l2_exits>2||
                   invept_count!=l2_exits||composed.tree.generation!=l2_exits+1||
                   gpa!=(l2_exits==0?0xf000u:l2_exits==1?0xe000u:0x208000u)||
                   rip!=label(l2_exits==0?nested_misconfig_fault0:l2_exits==1?nested_misconfig_fault1:nested_misconfig_fault2))break;
                reason=49;
                if(!nvmx_exit_store(&model,0x2400,gpa))break;
                log_text("NESTED.EPT12_MISCONFIG_REFLECTED_GPA=");log_hex(gpa);
                /* Intel defines GPA for misconfiguration. Qualification, GLA,
                 * instruction length and interruption information are not
                 * invented from the physical EPT02-miss exit. */
            } else if(reason==18) {
                if(l2_exits!=3||invept_count!=3||composed.tree.generation!=4||
                   rip!=label(nested_misconfig_done_call)||vmx_guest_regs.rax!=0x42||
                   !get(0x440c,&length)||length!=3)break;
                if(!nvmx_exit_store(&model,0x440c,length))break;
            } else break;
            if(!nvmx_exit_store(&model,0x681e,rip)||!nvmx_exit_store(&model,0x681c,rsp)||
               !nvmx_exit_store(&model,0x6820,flags)||!nvmx_exit_store(&model,0x4402,reason)||
               !nvmx_return_from_l2(&model)||!load(vmcs01)||!reflect_host(&profile))break;
