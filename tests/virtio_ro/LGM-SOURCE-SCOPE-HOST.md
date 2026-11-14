# Whole-source reference + actual LGM scoped host candidate

`lgm_source_scope.sh` reuses the isolated actual-LGM bounded/source-producer
candidate and actual PMM authority. It leaves default/shared sources untouched.
Unlike the earlier comparison fixture, every source callback and bp/q/private
value scratch copy executes inside session_scope's actual target pin/borrow.
Transfers consume only that synchronous private scratch. Zero initialization
also prepares scratch inside the scope, with no legacy map callback.

A minimal one-shot source owner holds private file-loaded image/initrd buffers
and a fixed command array before the FIRST candidate inspect. It remains held
through allocations, repeated inspect/load, EPT construction, failure rollback,
explicit release retry and epoch end. Allocation and source callback paths try
source replacement/retirement and verify refusal while held. Afterward, replacing
and reacquiring at identical addresses changes generation; the old reference
cannot admit spans or release the new reference.

This reference is API lifetime bookkeeping, not protection against arbitrary
writes through other aliases. The fixture has no user mappings/concurrent guest,
and file buffers are private and never mutated. Acquiring mutable external input
would still require a separately proven snapshot/immutable mapping boundary.
Generated producer classification remains trusted and not a public authority.

The controller and its saved operation handle are one-shot private fixture state;
the current target allocation capability comes from the fixed sidecar and is
then validated by session_scope. This is not yet a new public generation-bearing
LGM API: callbacks still accept PA and rely on the fixed, non-rebound sidecar.
Do not accept untrusted/stale PA requests or recreate fresh operation handles on
their behalf. Independent session_scope tests cover old operation/capability
rejection, not arbitrary kernel code fabricating current trusted metadata.

The normal run compares 32835 pages (128 MiB plus 67 EPT pages) with the original
actual LGM in the previous PMM epoch. Nine selected partial-operation failures
write a prefix, fault the scope, return after known synchronous completion, and
run actual LGM ownership rollback. A failed first-page release retains exactly
one actual PMM page until explicit retry. No guest is started, no partial bytes
are replayed as successful construction, and hardware-lifetime-pinned pages are
not admitted by this pre-publication single-lease profile.

This fixture does not inject NMI or prove native exclusion/hidden caches. The
separate session_scope fatal cases retain pin/borrow and exit; a real fatal
nonreturn would also leave this fixture's whole-source reference held because
no finally/recovery release exists. No native mapper is enabled.

After synchronous prepare returns, the fixture closes operation admission BEFORE
explicit owner release/retry and PMM epoch end. Failure's automatic rollback can
occur inside prepare before that return: in this single-thread fixture it has
no other caller, and injected transfer failure already latched faulted. This is
not a concurrent native cleanup protocol. Whole-source reference stays held
until all these steps finish.
