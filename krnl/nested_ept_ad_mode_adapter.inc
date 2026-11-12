            if(!composed_ready||contexts.poisoned||l2_exits>=6||rsp!=0x9000||
               (reason&(UINT64_C(1)<<31)))break;
            if(model.prepared){if(!nvmx_entry_commit(&model))break;}
            else if(!model.running)break;
            if(reason==48) {
                uint64_t q,gpa;struct nested_ept_result result;
                if(!get(0x6400,&q)||!get(0x2400,&gpa)||!(q&7)||
                   nested_ad_context_handle(&composed,contexts.active,gpa,q&7,&result)!=NESTED_AD_RETRY)break;
                sparse_fills++;continue;
            }
            static const uint64_t values[]={0x11,0x77,0x77,0x88,0x88,0x99};
            static const uint64_t keys[]={0x1005e,0x1005e,0x1001e,0x1001e,0x1005e,0x1005e};
            static const uint64_t leaf_a[]={0x8133,0x8333,0x8333,0x8033,0x8133,0x8333};
            if(reason!=18||rip!=label(nested_ad_mode_call)||!get(0x440c,&length)||length!=3||
               vmx_guest_regs.rax!=values[l2_exits]||selected12!=keys[l2_exits]||
               *(uint64_t *)ram(f,0x13078)!=leaf_a[l2_exits])break;
            log_text("NESTED.AD_MODE_CHECKPOINT=");log_hex(l2_exits);
            log_text("NESTED.AD_MODE_A_LEAF=");log_hex(leaf_a[l2_exits]);
            if(l2_exits==3) {
                for(unsigned i=0;i<4*4096;i+=8)if(*(uint64_t *)ram(f,0x10000+i)&0x300)goto done;
            }
            if((l2_exits==0||l2_exits>=4)&&(*(uint64_t *)ram(f,0x13020)&0x300)!=0x300)break;
            if(!nvmx_exit_store(&model,0x681e,rip)||!nvmx_exit_store(&model,0x681c,rsp)||
               !nvmx_exit_store(&model,0x6820,flags)||!nvmx_exit_store(&model,0x4402,reason)||
               !nvmx_exit_store(&model,0x440c,length)||!nvmx_exit_store(&model,0x4404,0)||
               !nvmx_return_from_l2(&model)||!load(vmcs01)||!reflect_host(&profile))break;
