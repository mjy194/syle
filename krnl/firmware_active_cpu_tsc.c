#include "firmware_active_cpu_tsc.h"
bool fcats_supported(const struct fcp_host*h){return fcad_supported(h)&&(h->basic_edx&16);}
bool fcats_query(const struct fcp_host*h,bool source,uint32_t leaf,uint32_t sub,struct fcp_result*out){
 if(!out||!source||!fcats_supported(h))return false;
 struct fcp_result r;if(!fcad_query(h,leaf,sub,&r))return false;
 if(leaf==1||(leaf>1&&leaf<0x80000000u)||leaf>0x80000008u)r.d|=16;
 *out=r;return true;
}
