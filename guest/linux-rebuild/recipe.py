"""Shared NEW fixture schema. No historical input identity is redefined."""
import hashlib
import json
from pathlib import Path
import re
SOURCE_SHA256="b1a2562be56e42afb3f8489d4c2a7ac472ac23098f1ef1c1e40da601f54625eb"
SOURCE_URL="https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.12.tar.xz"
FORMAT="syle-linux-rebuild-v2"
COMPRESSED_ORIGINAL_SHA256="f1d944fe81970b3b659d40ff561deab142c21648a0b5f2f38bc704e400b97389"
COMPRESSED_PATCHED_SHA256="d50d4f918e13ac1c3355ddfcaaf351958da86a5e6748eccdbb120fa68f81919d"
COMPRESSED_LINE=b"KBUILD_CFLAGS := -m$(BITS) -O2 $(CLANG_FLAGS)\n"
def apply_compat(source):
    target=Path(source)/"arch/x86/boot/compressed/Makefile"
    if digest(target)!=COMPRESSED_ORIGINAL_SHA256:
        raise ValueError("unexpected compressed Makefile; no patch applied")
    old=target.read_bytes()
    if old.count(COMPRESSED_LINE)!=1:raise ValueError("ambiguous compatibility context")
    target.write_bytes(old.replace(COMPRESSED_LINE,COMPRESSED_LINE[:-1]+b" -std=gnu11\n"))
    if digest(target)!=COMPRESSED_PATCHED_SHA256:raise ValueError("unexpected patched result")
    return {"path":"arch/x86/boot/compressed/Makefile",
            "before_sha256":COMPRESSED_ORIGINAL_SHA256,"after_sha256":digest(target),
            "patch_sha256":digest(Path(__file__).with_name("compressed-gnu11.patch"))}

def digest(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as f:
        for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
    return h.hexdigest()
def config(path):
    result={}
    for line in Path(path).read_text().splitlines():
        m=re.fullmatch(r'(CONFIG_[A-Z0-9_]+)=(.*)',line)
        if m:result[m[1]]=m[2]
        m=re.fullmatch(r'# (CONFIG_[A-Z0-9_]+) is not set',line)
        if m:result[m[1]]='n'
    return result
def check_config(fragment,resolved):
    request=config(fragment);actual=config(resolved)
    if not request:raise ValueError('empty configuration request')
    bad=[k for k,v in request.items() if actual.get(k,'n')!=v]
    if bad:raise ValueError('Kconfig did not honor requested settings: '+','.join(bad))
def load_build(folder):
    folder=Path(folder);b=json.loads((folder/'build.json').read_text())
    compat=b.get('compatibility',{})
    if compat.get('path')!='arch/x86/boot/compressed/Makefile' or compat.get('before_sha256')!=COMPRESSED_ORIGINAL_SHA256 or compat.get('patch_sha256')!=digest(Path(__file__).with_name('compressed-gnu11.patch')) or digest(folder/'compressed-gnu11.patch')!=compat.get('patch_sha256') or compat.get('after_sha256')!=COMPRESSED_PATCHED_SHA256:raise ValueError('invalid compatibility patch identity')
    if b.get('fragment_sha256')!=digest(Path(__file__).with_name('config.fragment')):raise ValueError('build recipe fragment differs from selected repository profile')
    if b.get('format')!=FORMAT or b.get('source_sha256')!=SOURCE_SHA256 or b.get('source_url')!=SOURCE_URL or b.get('status')!='built-not-runtime-validated':raise ValueError('unrecognized build/source identity')
    for name,key in [('bzImage','kernel_sha256'),('resolved.config','config_sha256'),('config.fragment','fragment_sha256')]:
        if digest(folder/name)!=b.get(key):raise ValueError('build artifact mismatch: '+name)
    check_config(folder/'config.fragment',folder/'resolved.config')
    tools=b.get('tools');env=b.get('environment')
    if not isinstance(tools,dict) or not isinstance(env,dict) or env.get('ARCH')!='x86_64' or env.get('KBUILD_BUILD_VERSION')!='1':raise ValueError('missing build provenance')
    for key in ('gcc','cc1','make','ld'):
        t=tools.get(key)
        if not isinstance(t,dict) or not isinstance(t.get('path'),str) or not t['path'].startswith('/') or not re.fullmatch('[0-9a-f]{64}',str(t.get('sha256',''))):raise ValueError('invalid tool provenance: '+key)
        if key!='cc1' and not t.get('version'):raise ValueError('missing tool version: '+key)
    return b

def check_archive_manifest(m):
    if m.get('format')!='syle-linux-baseline-v1':raise ValueError('invalid archive format')
    for k in ('archive_sha256','busybox_sha256','probe_sha256'):
        if not isinstance(m.get(k),str) or not re.fullmatch('[0-9a-f]{64}',m[k]):raise ValueError('invalid archive provenance: '+k)
