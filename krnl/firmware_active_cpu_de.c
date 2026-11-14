#include "firmware_active_cpu_de.h"
bool fcad_supported(const struct fcp_host*h){return fca_supported(h)&&(h->basic_edx&4);}
bool fcad_query(const struct fcp_host*h,uint32_t leaf,uint32_t sub,struct fcp_result*out){
 if(!out||!fcad_supported(h))return false;
 struct fcp_result r;if(!fca_query(h,leaf,sub,&r))return false;
 if(leaf==1||(leaf>1&&leaf<0x80000000u)||leaf>0x80000008u)r.d|=4;
 *out=r;return true;
}
