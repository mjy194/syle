from pathlib import Path
s=Path(__file__).resolve().parents[1]
a=(s/'linux_cpu_v7.c').read_text()
b=(s/'linux_cpu_v8.c').read_text()
assert a.count('index<16')==1 and a.count('index==15')==4
assert b==a.replace('index<16','index<64').replace('index==15','index==63').replace('max16_entries','max64_entries')
assert list(range(64))[-1]==63 and len(range(64))==64
assert all(i!=63 for i in range(63)) and not (64<64)
print('cpu_v8_budget: PASS (exact budget-only source transformation; not VM execution)')
