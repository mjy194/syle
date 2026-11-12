            /* Reusable decision takes actual VMCS12 filters and actual PFEC.
             * Labels and counters below are diagnostic assertions only. */
            extern const unsigned char nested_pf_route_fault0[],nested_pf_route_fault1[],nested_pf_route_fault2[],nested_pf_route_fault3[];
            extern const unsigned char nested_pf_route_call0[],nested_pf_route_call1[],nested_pf_route_call2[],nested_pf_route_call3[];
            const unsigned char *faults[]={nested_pf_route_fault0,nested_pf_route_fault1,nested_pf_route_fault2,nested_pf_route_fault3};
            const unsigned char *calls[]={nested_pf_route_call0,nested_pf_route_call1,nested_pf_route_call2,nested_pf_route_call3};
            const unsigned expected_error[]={0,2,2,0};
            if(event_calls>=4)break;
            if(model.prepared){if(!nvmx_entry_commit(&model))break;}
            else if(!model.running)break;
            uint64_t info=0,error=0,pending,bitmap,mask,match;
            if(!get(0x4016,&pending)||!nvmx_peek(&model,0x4004,&bitmap)||
               !nvmx_peek(&model,0x4006,&mask)||!nvmx_peek(&model,0x4008,&match))break;
            if(l1_event.armed) {
                if(reason!=18||rip!=label(calls[event_calls])||vmx_guest_regs.r13!=event_calls+1||
                   vmx_guest_cr2!=0xf000||!nested_entry_observe(&l1_event,true)||
                   !put(0x4016,pending&~UINT64_C(0x80000000)))break;
                if(l1_event.origin==NESTED_ORIGIN_L1)l1_deliveries++;
                pending&=~UINT64_C(0x80000000);
                log_text("NESTED.PF_HANDLER_CONFIRMED_STAGE=");log_hex(event_calls);
            }
            if(reason==0) {
                uint64_t gla;
                if(!get(0x4404,&info)||info!=0x80000b0e||!get(0x4406,&error)||
                   !get(0x6400,&gla))break;
                struct nested_event_input input={.exit_reason=0,.exit_info=info,.exit_error=error,
                    .vectoring=vectoring,.entry_pending=pending,.blocking=blocking,.pending_debug=debug,
                    .rflags=flags,.bitmap12=bitmap,.bitmap_l0=L0_EVENT_BITMAP,
                    .pf_mask12=mask,.pf_match12=match};
                struct nested_event_injection injection;
                enum nested_event_action action=nested_event_route(&input,&injection);
                if(action==NESTED_EVENT_REJECT)break;
                if(gla!=0xf000||rip!=label(faults[event_calls])||rsp!=0x9000||
                   error!=expected_error[event_calls]||vmx_guest_cr2!=0x22220000+event_calls)break;
                log_text("NESTED.PF_ROUTE_STAGE=");log_hex(event_calls);
                log_text("NESTED.PF_ROUTE_BITMAP=");log_hex(bitmap);
                log_text("NESTED.PF_ROUTE_MASK=");log_hex(mask);
                log_text("NESTED.PF_ROUTE_MATCH=");log_hex(match);
                log_text("NESTED.PF_ROUTE_ACTUAL_ERROR=");log_hex(error);
                if(action==NESTED_EVENT_REINJECT_L2) {
                    struct nested_entry_request request={.info=injection.info,.error=injection.error,
                        .length=0,.blocking=blocking,.vectoring=vectoring,.pending_debug=debug,.rflags=flags};
                    if(event_calls>=2||!nested_entry_arm(&l1_event,NESTED_ORIGIN_L0,&request)||
                       !put(0x4018,injection.error)||!put(0x401a,0)||!put(0x4016,injection.info))break;
                    /* Direct PF did not update CR2. L0 is the injecting owner
                     * here, so install the actual qualification, never hostCR2. */
                    vmx_guest_cr2=gla;internal_uds++;
                    log_text("NESTED.PF_OWNER_L0_SET_CR2_AND_REINJECT\n");continue;
                }
                if(event_calls<2||!nvmx_exit_store(&model,0x6400,gla)||
                   !nvmx_exit_store(&model,0x4406,error))break;
                reflected_uds++;
                log_text("NESTED.PF_OWNER_L1_KEEP_CR2_AND_REFLECT\n");
            } else if(reason==18) {
                if(!l1_event.observed||rip!=label(calls[event_calls])||
                   !get(0x440c,&length)||length!=3||!nvmx_exit_store(&model,0x440c,length))break;
                event_calls++;
            } else break;
            if(!nvmx_exit_store(&model,0x681e,rip)||!nvmx_exit_store(&model,0x681c,rsp)||
               !nvmx_exit_store(&model,0x6820,flags)||!nvmx_exit_store(&model,0x4402,reason)||
               !nvmx_exit_store(&model,0x4404,info)||!nvmx_return_from_l2(&model)||
               !load(vmcs01)||!reflect_host(&profile))break;
            if(l1_event.armed&&!nested_entry_release(&l1_event))break;
