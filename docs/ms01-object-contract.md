# MS-01 assembled exception-entry contract

Run the focused host checks with:

```sh
python3 ms01/run.py --only object-contract,vector-layout,suite-contract
```

`object-contract` needs Python 3, GNU-compatible `as` and `objdump`, and a C11
compiler supporting `-m64`. `AS`, `OBJDUMP` and `CC` may select those tools.
Do not use Python optimization: the script explicitly rejects disabled assertions.
The test assembles production `efi/src/exceptions.S` in a temporary directory;
it never links, loads or executes that exception code. All generated files are
removed after the invocation. The aggregate captures its JSON stdout in the log.

Coverage:

- All 256 vector immediates, their encoded sign extension and error-code
  normalization, transfer to the common entry, and ordered table relocations.
- Saved GPR order and the original frame pointer before stack alignment.
- Two actual production `struct frame` declarations, extracted from the EFI
  exception consumer and kernel Ring 3 consumer: 44 compiled offset assertions
  and 2 size assertions (RIP offset 136, frame size 176).
- Five independently assembled malformed fixtures must be rejected: vector 128
  sign extension, wrong #GP vector, extra stack push, extra stack adjustment,
  and displaced vector-table slot. Both frame consumers also reject a swapped
  error/RIP field fixture at compile time.

The existing SC-M1-045 and SC-M1-049 links include this host suite. No scenario
selection, milestone boundary or historical result is changed by those links.

These checks inspect object code and extracted declarations, not complete C
translation units or live CPU delivery. They do not prove the runtime saved RIP,
NMI reentry behavior, physical hardware, or full milestone acceptance. Historical
noncanonical-RIP evidence remains historical; this test does not convert it to a
pass. No held runtime supplement is resumed. Production code is unchanged.
