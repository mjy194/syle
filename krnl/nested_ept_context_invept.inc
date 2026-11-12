            if(!model.root||contexts.poisoned||(op.ea&15))break;
            uint64_t *descriptor=(void *)memory,type=r[op.regB];
            /* Narrow descriptor-high policy: unsupported, not fabricated #GP
             * or architectural error28. Type2 deliberately ignores low64. */
            if(descriptor[1])break;
            enum nested_context_status status=nested_ept_contexts_invalidate(&contexts,type,descriptor[0]);
            if(status==NESTED_CONTEXT_INVALID) {
                struct nvmx_result failed=nvmx_invept_result(&model,false,flags);
                if(!put(0x6820,failed.flags)||!put(0x681e,rip+length))break;
                continue;
            }
            if(status!=NESTED_CONTEXT_OK)break;
            struct nvmx_result flushed=nvmx_invept_result(&model,true,flags);
            if(flushed.status!=NVMX_SUCCESS||!put(0x6820,flushed.flags)||!put(0x681e,rip+length))break;
            invept_count++;
            log_text("NESTED.CONTEXT_INVEPT_COMPLETED_TYPE=");log_hex(type);
            continue;
