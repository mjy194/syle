#!/usr/bin/env python3
import pathlib,sys
repo=pathlib.Path(__file__).resolve().parents[2];out=pathlib.Path(sys.argv[1]);out.parent.mkdir(parents=True,exist_ok=True)
s=(repo/'efi/src/exceptions.c').read_text();assert 'f->vector != 2 && ring3_fault' in s
assert 'exceptions_owned_context' not in s
if '--host-bootstrap' in sys.argv:
 s=s.replace('void exceptions_init(void) {','extern void ecg_test_cli_cld(void);\nextern void ecg_test_ltr(UINT16);\nextern void ecg_test_lidt(const struct table_ptr *);\nvoid exceptions_init(void) {')
 for old,new in [('__asm__ volatile("cli; cld" ::: "memory");','ecg_test_cli_cld();'),('__asm__ volatile("ltr %0" : : "r"(selector));','ecg_test_ltr(selector);'),('__asm__ volatile("lidt %0" : : "m"(idtr) : "memory");','ecg_test_lidt(&idtr);')]:
  assert s.count(old)==1;s=s.replace(old,new)
out.write_text(s+'\n'+(repo/'tests/virtio_ro/exception_context_gate.inc').read_text())
