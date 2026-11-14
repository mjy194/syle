#include "backend.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct mem {uint8_t b[32768];unsigned writes,fail_at,barriers,reads,read_fail,events;uint64_t event[256];};
static uint8_t disk[1024];
static bool admit(void *p,uint64_t a,size_t n,bool w){(void)p;(void)w;return a<=32768 && n<=32768-a;}
static bool rd(void *p,uint64_t a,void *b,size_t n){struct mem *m=p;if(++m->reads==m->read_fail)return false;if(!admit(p,a,n,false))return false;memcpy(b,((struct mem*)p)->b+a,n);return true;}
static bool wr(void *p,uint64_t a,const void *b,size_t n){struct mem *m=p;assert(m->events<256);m->event[m->events++]=a;m->writes++;if(!admit(p,a,n,true))return false;memcpy(m->b+a,b,n);return m->writes!=m->fail_at;}
static void barrier(void *p){struct mem *m=p;assert(m->events<256);m->event[m->events++]=UINT64_MAX;m->barriers++;}
static void put(struct mem *m,unsigned a,uint64_t v,unsigned n){for(unsigned i=0;i<n;i++)m->b[a+i]=(uint8_t)(v>>(i*8));}
static unsigned get(struct mem *m,unsigned a){return (uint32_t)m->b[a]|(uint32_t)m->b[a+1]<<8|(uint32_t)m->b[a+2]<<16|(uint32_t)m->b[a+3]<<24;}
static void desc(struct mem *m,unsigned i,uint64_t a,unsigned n,unsigned f,unsigned next){unsigned p=4096+16*i;put(m,p,a,8);put(m,p+8,n,4);put(m,p+12,f,2);put(m,p+14,next,2);}
static void setup(struct vro *s,struct mem *m,unsigned kind,unsigned bytes){memset(m,0,sizeof(*m));struct vro_io io={m,admit,rd,wr,barrier};assert(vro_init(s,io,disk,sizeof(disk),1));put(m,6144,1,2);put(m,6146,1,2);desc(m,0,16384,16,1,1);desc(m,1,17408,bytes,kind==1?1:3,2);desc(m,2,19456,1,2,0);put(m,16384,kind,4);m->b[19456]=99;}
int main(void){struct vro s;struct mem m;for(unsigned i=0;i<sizeof(disk);i++)disk[i]=(uint8_t)i;
 setup(&s,&m,0,512);assert(vro_poll(&s)==1);assert(!memcmp(m.b+17408,disk,512));assert(m.b[19456]==0&&get(&m,8200)==513&&m.barriers==1);assert(vro_poll(&s)==0&&m.writes==4);assert(m.events==5&&m.event[0]==17408&&m.event[1]==19456&&m.event[2]==8196&&m.event[3]==UINT64_MAX&&m.event[4]==8194);
 setup(&s,&m,8,512);assert(vro_poll(&s)==1&&get(&m,8200)==20);
 setup(&s,&m,8,20);assert(vro_poll(&s)==1&&get(&m,8200)==21);assert(!memcmp(m.b+17408,"SYLE-READONLY-DISK-1",20));
 setup(&s,&m,8,10);assert(vro_poll(&s)==1&&get(&m,8200)==0&&m.b[19456]==1);
 setup(&s,&m,0,512);put(&m,16392,UINT64_MAX,8);assert(vro_poll(&s)==1&&get(&m,8200)==0&&m.b[19456]==1);
 setup(&s,&m,1,512);assert(vro_poll(&s)==1&&get(&m,8200)==1&&m.b[19456]==1);for(unsigned i=0;i<sizeof(disk);i++)assert(disk[i]==(uint8_t)i);
 setup(&s,&m,99,512);assert(vro_poll(&s)==1&&get(&m,8200)==0&&m.b[19456]==2);
 for(unsigned f=1;f<=4;f++){setup(&s,&m,0,512);m.fail_at=f;assert(vro_poll(&s)==-1&&s.failed);unsigned w=m.writes;assert(vro_poll(&s)==-1&&m.writes==w);}
 setup(&s,&m,0,512);desc(&m,1,6144,512,3,2);assert(vro_poll(&s)==-1&&!m.writes);
 setup(&s,&m,0,512);desc(&m,1,UINT64_MAX-100,512,3,2);assert(vro_poll(&s)==-1&&!m.writes);
 setup(&s,&m,0,512);desc(&m,1,17408,512,3,1);assert(vro_poll(&s)==-1&&!m.writes);
 setup(&s,&m,0,512);desc(&m,1,16384,512,3,2);assert(vro_poll(&s)==-1&&!m.writes);
 setup(&s,&m,0,512);desc(&m,1,17408,512,7,2);assert(vro_poll(&s)==-1&&!m.writes);
 setup(&s,&m,0,512);put(&m,6146,2,2);assert(vro_poll(&s)==-1&&!m.writes);
 setup(&s,&m,0,512);s.index=65535;put(&m,6146,0,2);put(&m,6148+2*127,0,2);assert(vro_poll(&s)==1&&s.index==0&&m.b[8194]==0&&m.b[8195]==0);
 for(unsigned f=1;f<=6;f++){setup(&s,&m,0,512);m.read_fail=f;assert(vro_poll(&s)==-1&&s.failed&&!m.writes);}
 setup(&s,&m,0,512);desc(&m,1,17408,256,3,3);desc(&m,3,17920,256,3,2);assert(vro_poll(&s)==1&&get(&m,8200)==513);assert(!memcmp(m.b+17408,disk,256)&&!memcmp(m.b+17920,disk+256,256));
 setup(&s,&m,8,512);desc(&m,1,17408,8,3,3);desc(&m,3,17920,12,3,2);assert(vro_poll(&s)==1&&get(&m,8200)==21);
 setup(&s,&m,99,512);desc(&m,1,17408,512,3,3);desc(&m,3,18432,4,1,2);assert(vro_poll(&s)==-1&&!m.writes);
 setup(&s,&m,0,512);put(&m,6144,0,2);assert(vro_poll(&s)==-1&&!m.writes);
 setup(&s,&m,0,512);put(&m,20000,UINT32_MAX,4);assert(get(&m,20000)==UINT32_MAX);
 puts("virtio-ro host pilot: PASS (no transport/guest validation)");return 0;
}
