#include "firmware_pci_bus.h"
void vfb_abort(struct vfb*s){if(s){s->failed=true;s->board.failed=true;s->disk.failed=true;s->disk.backend.failed=true;}}
static bool canonical(uint64_t a){return (a>>47)==0||(a>>47)==0x1ffff;}
bool vfb_init(struct vfb*s,struct vro_io io,const uint8_t*d,size_t n,uint64_t e,uint64_t r,uint64_t gap){
 if(!s)return false;
 /* Fresh zeroed object only, never reset/rebind a live device. */
 if(s->failed||s->busy||s->board.pm.ready||s->disk.disk)return false;
 if(!fpp_init(&s->board,e,r,gap)||!vrt_init(&s->disk,io,d,n)){vfb_abort(s);return false;}return true;
}
bool vfb_init_projected(struct vfb*s,struct vro_io io,const uint8_t*d,size_t n,uint64_t e,uint64_t r,uint64_t gap){
 if(!vfb_init(s,io,d,n,e,r,gap))return false;
 s->projection=(struct vpc){0};s->projected=true;return true;
}
bool vfb_io(struct vfb*s,uint64_t g[15],uint64_t rip,unsigned cpl,uint64_t q,unsigned len,uint64_t epoch,uint64_t ref,const uint8_t*b,unsigned available,bool exclusive,void*ctx,bool(*wr)(void*,uint32_t,uint64_t)){
 if(!s||s->failed||!exclusive)return false;
 if(s->busy){vfb_abort(s);return false;}s->busy=true;
 unsigned z=q&7,w=z==0?1:z==1?2:z==3?4:0,port=(q>>16)&0xffff;bool input=!!(q&8);
 uint32_t addr=s->board.pci.address;
 unsigned bdf=(addr>>8)&0xffff;
 bool projected=s->projected&&(bdf==0||bdf==8||bdf==11);
 bool cfg=port>=0xcfc&&port<=0xcff&&(addr&0x80000000u)&&(bdf==16||projected);
 if(s->projected&&port==0xb008&&!vpc_pm_decode(&s->projection,&s->board.pm)){vfb_abort(s);s->busy=false;return false;}
 bool bar=s->disk.bar&&!s->disk.probe&&(s->disk.command&1)&&port>=s->disk.bar&&port-s->disk.bar<64;
 if(!cfg&&!bar){bool ok=fpp_io(&s->board,g,rip,cpl,q,len,epoch,ref,b,available,true,ctx,wr);s->busy=false;if(!ok||s->failed){vfb_abort(s);return false;}return true;}
 if(!fpm_sample(&s->board.pm,epoch,ref)||!fpp_encoding(q,len,b,available)||!g||!wr||cpl||!canonical(rip)||rip>UINT64_MAX-len||!canonical(rip+len)||
 (!(q&64)&&(uint16_t)g[3]!=port)){vfb_abort(s);s->busy=false;return false;}
 struct vrt next=s->disk;struct vpc config=s->projection;struct fpm_state pm=s->board.pm;uint32_t value=(uint32_t)g[0];if(w<4)value&=(1u<<(8*w))-1;
 bool ok;
 if(cfg){
  unsigned off=(addr&0xfc)+(port-0xcfc);
  if((port&(w-1))||w>0xd00-port){vfb_abort(s);s->busy=false;return false;}
  if(!projected&&!input&&off==16&&w==4&&value!=UINT32_MAX){uint32_t base=value&0xffffffc0u;
   if(base&&(base<0xd000||base>0xdfc0)){vfb_abort(s);s->busy=false;return false;}}
  ok=projected?vpc_config(&config,&pm,(uint16_t)bdf,off,w,!input,&value)==1:vrt_config(&next,off,w,!input,&value);
 }else{
  /* Refuse stale queue publication across device reset, without clearing RAM. */
  if(!input&&port-next.bar==8&&w==4&&value){uint8_t a[2],u[2];uint64_t base=(uint64_t)value<<12;
   if(!next.io.read(next.io.ctx,base+2050,a,2)||!next.io.read(next.io.ctx,base+4098,u,2)||a[0]||a[1]||u[0]||u[1]){vfb_abort(s);s->busy=false;return false;}}
  ok=vrt_io(&next,(uint16_t)port,w,!input,&value);
 }
 if(!ok||s->failed||!wr(ctx,0x681e,rip+len)||s->failed){vfb_abort(s);s->busy=false;return false;}
 s->disk=next;s->projection=config;s->board.pm=pm;
 if(input){uint64_t mask=w==4?UINT32_MAX:(UINT64_C(1)<<(8*w))-1;g[0]=w==4?value:(g[0]&~mask)|value;}
 s->busy=false;return true;
}
