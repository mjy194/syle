#include "ad_entry_merge.h"
struct ad_entry_result ad_entry_merge(const struct ad_entry_backend *b,void *ctx,
                                      const struct ad_entry_request *r)
{
    struct ad_entry_result out={.status=AD_ENTRY_INVALID};
    if(!b||!b->guard||!b->load||!b->compare_exchange||!r||!r->attempts||
       !r->stamp.domain_id||!r->stamp.incarnation||!r->stamp.context_id||
       !r->stamp.epoch||!r->stamp.lifetime||
       (r->mutable_ad!=AD_ENTRY_A&&r->mutable_ad!=(AD_ENTRY_A|AD_ENTRY_D))||
       !r->requested_bits||(r->requested_bits&~r->mutable_ad)||
       (r->expected_identity&r->mutable_ad))return out;
    for(unsigned i=0;i<r->attempts;i++) {
        if(!b->guard(ctx,r->stamp)){out.status=AD_ENTRY_STALE;return out;}
        uint64_t current=b->load(ctx);
        if((current&~r->mutable_ad)!=r->expected_identity){out.status=AD_ENTRY_REWALK;return out;}
        uint64_t desired=current|r->requested_bits;
        if(desired==current) {
            out.status=b->guard(ctx,r->stamp)?AD_ENTRY_DONE:AD_ENTRY_STALE;
            return out;
        }
        /* Guard does not lock writers. An intervening non-A/D change must
         * defeat CAS; an ABA needs a real external lifetime/version protocol. */
        if(!b->guard(ctx,r->stamp)){out.status=AD_ENTRY_STALE;return out;}
        ++out.comparisons;
        if(b->compare_exchange(ctx,current,desired)) {
            out.changed=true;
            out.status=b->guard(ctx,r->stamp)?AD_ENTRY_DONE:AD_ENTRY_APPLIED_BUT_STALE;
            return out;
        }
    }
    out.status=b->guard(ctx,r->stamp)?AD_ENTRY_RETRY:AD_ENTRY_STALE;
    return out;
}
