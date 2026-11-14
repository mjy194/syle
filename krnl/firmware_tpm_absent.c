#include "firmware_tpm_absent.h"
bool fta_mmio(uint64_t gpa,unsigned width,bool write,uint64_t*value){
 if(!value||gpa!=UINT64_C(0xfed40000)||width!=1||write)return false;
 *value=UINT64_C(0xff);return true;
}
