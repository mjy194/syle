#include "linux_progress_diag.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static bool safe(void*p){return *(bool*)p;}
static unsigned reads;static bool rd(void*p,uint32_t f,uint64_t*v){(void)p;reads++;*v=f;return true;}
int main(void){struct lprog_gate g={0};unsigned logs=0;for(uint64_t n=0;n<300000;n++){if(lprog_due(&g,n,false,false))logs++;if(lprog_due(&g,n+1,true,true))logs++;}assert(logs==128&&g.samples==64&&g.records==128);assert(lprog_mark(&g,1));assert(!lprog_mark(&g,1));assert(lprog_mark(&g,2));assert(lprog_mark(&g,4));assert(g.records==131&&!lprog_mark(&g,7));
 g=(struct lprog_gate){0};assert(lprog_due(&g,4095,false,false));assert(!lprog_due(&g,4095,true,false));assert(lprog_due(&g,8191,false,false));assert(!lprog_due(&g,8191,true,true));assert(!lprog_due(&g,UINT64_MAX,false,false));
 static struct lgd_context d;static struct lgs_session s;bool yes=true;d.configured=d.ready=true;s.prepared=s.host.active=s.host.ready=s.state.ready=true;s.platform.safe=safe;s.platform.ctx=&yes;s.platform.read_field=rd;d.session=&s;struct lprog_snapshot out;struct lgd_context before=d;assert(lprog_collect(&d,&s,&out)&&out.vmcs_valid&&reads==2&&!memcmp(&d,&before,sizeof d));s.state.host_dirty=true;assert(!lprog_collect(&d,&s,&out)&&reads==2);s.state.host_dirty=false;yes=false;assert(!lprog_collect(&d,&s,&out)&&reads==2);yes=true;d.busy=true;assert(!lprog_collect(&d,&s,&out));puts("progress sampler host PASS");}
