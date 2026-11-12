            if(!composed_ready||composed.tree.poisoned||l2_exits>=4||rsp!=0x9000||
               (reason&(UINT64_C(1)<<31)))break;
            if(model.prepared){if(!nvmx_entry_commit(&model))break;}
            else if(!model.running)break;
            if(reason==48) {
                uint64_t q,gpa;
                if(!get(0x6400,&q)||!get(0x2400,&gpa)||!(q&7)||
                   !nested_ept_ad_hw_fault(&ad_cache,&composed,gpa,q&7))break;
                sparse_fills++;
                log_text("NESTED.AD_INTERNAL_ACCESS_GPA=");log_hex(gpa);
                log_text("NESTED.AD_INTERNAL_ACCESS_MASK=");log_hex(q&7);
                continue; /* includes native AD-enabled guest paging writes */
            }
            const unsigned char *calls[]={nested_ad_read_call,nested_ad_write_call,nested_ad_reread_call,nested_ad_rewrite_call};
            uint64_t data=*(uint64_t *)ram(f,0x12008),paging=*(uint64_t *)ram(f,0x13020);
            if(reason!=18||rip!=label(calls[l2_exits])||!get(0x440c,&length)||length!=3||
               (data&0x300)!=(l2_exits&1?0x300u:0x100u)||(paging&0x300)!=0x300||
               !(*(uint64_t *)ram(f,0x10000)&0x100)||!(*(uint64_t *)ram(f,0x11000)&0x100)||
               !(*(uint64_t *)ram(f,0x12000)&0x100)||
               (!(l2_exits&1)&&vmx_guest_regs.rax!=(l2_exits?0x77u:0x11u)))break;
            log_text("NESTED.AD_CHECKPOINT=");log_hex(l2_exits);
            log_text("NESTED.AD_LARGE_LEAF=");log_hex(data);
            log_text("NESTED.AD_PAGING_LEAF=");log_hex(paging);
            if(!nvmx_exit_store(&model,0x681e,rip)||!nvmx_exit_store(&model,0x681c,rsp)||
               !nvmx_exit_store(&model,0x6820,flags)||!nvmx_exit_store(&model,0x4402,reason)||
               !nvmx_exit_store(&model,0x440c,length)||!nvmx_exit_store(&model,0x4404,0)||
               !nvmx_return_from_l2(&model)||!load(vmcs01)||!reflect_host(&profile))break;
