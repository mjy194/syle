# N46 plan and dependency assessment

Accepted scope: a separately opted-in, fixed dedicated UC data page with 4 KiB
EPT12 leaves, IPAT1 and AD1. EPT12 tables, EPTP walks and every other backing page
remain WB. One immutable catalog target GPA/HPA and a matching fixed-type lease
bind the UC authority. Non-WB executable or table backing, combined type masks,
WT/WC/WP, IPAT0, non-WB huge leaves and live type changes remain rejected.

N46A first integrates the production decoder, authority catalog, fresh permits,
durable trace validation, typed owner bridge and stopped-owner A/D maintenance.
Tests use owned synthetic storage and mocked physical/cache primitives, never
claiming physical UC behavior. The type epoch remains fixed while A/D epochs
advance and private caches are invalidated/retracked. Existing APIs stay WB-only.

N46B is a separate actual-guest profile, after N46A review. Its physical adapter
must prove both CPUs' existing PAT/MTRR/CR0 state, unique host alias and complete
EPT alias inventory, then establish UC with break-before-make/TLB/cache receipts
before source publication or any guest entry. The old AD0 transition path is not
AD1 evidence and cannot simply be enabled: it changes EPT12 structural type.
All target/source backing is retained after teardown, with no reverse-transition
or release claim. No new live type transition is part of N46.

Dynamic EPT12 updates are not a dependency: source structures and target type stay
immutable for the entire retained lifetime. Existing source pins, updater leases,
owner-local physical reference removal, exact owner flush receipts and stopped
cross-core admission remain required. Two reset/read/first-write rounds must show
private dirty tracking independent of another owner's shared D.

Software acceptance: positive mixed WB/UC paths, target/source mismatch and all
unsupported format/source-class refusals, per-owner first-write promotion,
two reset/retrack cycles, stale epoch/cache/pin/receipt attacks, sanitizers and
assertion-killed weakening mutants, plus actual freestanding compilation/linking.
Full regression and hardware acceptance are parent-coordinated only after review.

Prior frozen trees and evidence are immutable. No Codex/GitHub, host settings,
installs, DMA or other VMs are authorized by this work.
