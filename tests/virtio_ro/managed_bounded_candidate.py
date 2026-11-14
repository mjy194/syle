#!/usr/bin/env python3
import pathlib,sys
s=pathlib.Path(sys.argv[1]).read_text()
a='const struct lgs_prepare_path path={b->allocate_role,lgm_prepare_roles,lvh_open_roles};'
assert s.count(a)==1
s=s.replace(a,'const struct lgs_prepare_path path={b->allocate_role,cb_lgm_prepare_roles,mbp_host_open};')
a='if(lgs_prepare_with_path(b->session,profile,&path))return 1;'
assert s.count(a)==1
s=s.replace(a,'int result=lgs_prepare_with_path(b->session,profile,&path);\n bool done=mbp_prepare_done(b->context);\n if(result||!done)return 1;')
s=s.replace('#include "linux_guest_role_binding.h"','#include "managed_bounded_candidate.h"')
for name in ['lgr_configure_barrier','lgr_configure','lgr_guest_backend']:
 s=s.replace(name,name.replace('lgr_','mbp_'))
pathlib.Path(sys.argv[2]).write_text(s)
