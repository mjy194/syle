#include "firmware_active_cpu.h"
bool fca_supported(const struct fcp_host*h){return fcp_supported(h)&&(h->basic_edx&(1u<<9));}
bool fca_query(const struct fcp_host*h,uint32_t leaf,uint32_t sub,struct fcp_result*out){
 if(!out||!fca_supported(h))return false;
 struct fcp_result r;if(!fcp_query(h,leaf,sub,&r))return false;
 if(leaf==1||(leaf>1&&leaf<0x80000000u)||leaf>0x80000008u)r.d|=1u<<9;
 *out=r;return true;
}
