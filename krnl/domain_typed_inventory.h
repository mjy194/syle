#ifndef SYLE_DOMAIN_TYPED_INVENTORY_H
#define SYLE_DOMAIN_TYPED_INVENTORY_H
#include <stdbool.h>
#include <stdint.h>
#define DOMAIN_TYPED_ROOTS 16u
#define DOMAIN_TYPED_ALIASES 16u
struct domain_typed_root_binding {
    uint64_t id,generation,eptp,historical_mask;
};
struct domain_typed_alias {
    unsigned root_index;
    uint64_t gpa,hpa,raw;
    uint8_t permissions,memory_type;
    bool ignore_pat;
};
struct domain_typed_root_inventory {
    struct domain_typed_root_binding binding;
    uint64_t visited_nodes,scan_digest; /* noncryptographic full-scan change detector */
    unsigned aliases;
};
struct domain_typed_inventory {
    uint64_t target_hpa,total_nodes;
    unsigned root_count,alias_count;
    struct domain_typed_root_inventory roots[DOMAIN_TYPED_ROOTS];
    struct domain_typed_alias aliases[DOMAIN_TYPED_ALIASES];
};
struct domain_typed_request {
    const struct domain_typed_root_binding *roots;
    unsigned root_count,maxphys;
    uint64_t target_hpa,node_budget;
};
/* Monitor-owned whole 4K aligned WB table, pinned and immutable for entire
 * audit. Caller has stopped ALL consumers/writers, including A/D hardware.
 * No mapping is inferred from guest data or a bare numerical host pointer. */
typedef bool (*domain_typed_table_resolver)(void *,uint64_t hpa,const uint64_t **view);
struct domain_typed_frame {
    const uint64_t *view;
    uint64_t hpa,gpa_prefix;
    unsigned slot;
    uint8_t permissions;
};
struct domain_typed_workspace {
    struct domain_typed_inventory result;
    struct domain_typed_frame frames[4];
};
enum domain_typed_status {
    DOMAIN_TYPED_OK,DOMAIN_TYPED_INVALID,DOMAIN_TYPED_UNSUPPORTED,
    DOMAIN_TYPED_OWNERSHIP,DOMAIN_TYPED_BUDGET,DOMAIN_TYPED_CAPACITY
};
/* Read-only exhaustive walk: WB AD0 four-level EPTP, 4K leaves, no huge pages,
 * ignored/reserved features rejected. Target leaves require IPAT1, WB or UC,
 * and no execute permission; every other leaf must WB. Nonpresent slots are
 * not followed. Every reached table's ALL 512 entries are examined.
 * Shared subtrees are scanned for each path; cycles on a path fail.
 * Node budget counts repeated visits, including roots with zero target alias.
 * Caller workspace and out must not overlap each other, roots or audited RAM.
 * Failure leaves out unchanged. Counts/identities are not cryptographic proof;
 * this does not prove CPU cache state, quiescence, data roles or UC suitability. */
enum domain_typed_status domain_typed_inventory_audit(
    const struct domain_typed_request *,domain_typed_table_resolver,void *,
    struct domain_typed_workspace *,struct domain_typed_inventory *out);
/* Explicit target-only non-WB policy: 0=UC, 4=WT. Never admits both.
 * Legacy audit above remains UC-only; all table/non-target rules unchanged. */
enum domain_typed_status domain_typed_inventory_audit_type(
    const struct domain_typed_request *,unsigned nonwb_type,
    domain_typed_table_resolver,void *,struct domain_typed_workspace *,
    struct domain_typed_inventory *);
#endif
