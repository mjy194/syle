"""Generate isolated pre-prepare observation copy; NEVER edits shared source."""
import pathlib,sys
s=pathlib.Path(sys.argv[1]).read_text();a=s.index('static int prepare(void*c,uint64_t p){');b=s.index('static void diagnostic_field(',a)
old=s[a:b];prefix=old[:old.index(' uint16_t incoming_ss;')]
new=prefix.replace('static int prepare(', 'static int prepare_context_candidate(')+''' struct mhc_saved saved=enter_context();
 source_prepare_observe();
 /* Diagnostic-only: NEVER enter lgs_prepare/VMX, even if window is missed. */
 leave_context(&saved);return 1;
}
'''
assert 'lgs_guest_backend.prepare(c,p)' in old
s=s[:a]+'extern void source_prepare_observe(void);\n'+new+s[b:]
assert s.count('.prepare=prepare,')==1
s=s.replace('.prepare=prepare,','.prepare=prepare_context_candidate,')
assert 'lgs_guest_backend.prepare' not in s
pathlib.Path(sys.argv[2]).write_text(s)
if len(sys.argv)==4:pathlib.Path(sys.argv[3]).write_text(new)
