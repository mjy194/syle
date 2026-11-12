#include "../simple_guest.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned char elf[16384];
static void put(unsigned off, uint64_t v,unsigned n) { for(unsigned i=0;i<n;++i) elf[off+i]=(unsigned char)(v>>(i*8)); }
static void fixture(void) {
 memset(elf,0,sizeof elf); memcpy(elf,"\177ELF",4); elf[4]=2;elf[5]=1;elf[6]=1;
 put(16,2,2);put(18,62,2);put(20,1,4);put(24,0x10000,8);put(32,64,8);put(52,64,2);put(54,56,2);put(56,1,2);
 put(64,1,4);put(68,5,4);put(72,4096,8);put(80,0x10000,8);put(88,0x10000,8);put(96,4098,8);put(104,8193,8);put(112,4096,8);
 for(unsigned i=4096;i<8194;++i) elf[i]=(unsigned char)(i*7+3);
}
static void reject(void) { struct sg_image a,b;memset(&a,0xa5,sizeof a);b=a;assert(!sg_image_inspect(elf,sizeof elf,&a));assert(!memcmp(&a,&b,sizeof a)); }
int main(void) {
 struct sg_image im;
 assert(!sg_image_inspect((const void *)(UINTPTR_MAX-31),64,&im));
 fixture();assert(sg_image_inspect(elf,sizeof elf,&im));assert(im.count==1);assert(sg_image_executable(&im,0x10fff,3));assert(!sg_image_executable(&im,0x11001,2));assert(!sg_image_executable(&im,0x11002,1));assert(!sg_image_executable(&im,UINT64_MAX,2));assert(!sg_image_executable(&im,0x10000,UINT64_MAX));assert(!sg_image_executable(&im,0x10000,0));
 for(unsigned i=0;i<64;++i) assert(!sg_image_inspect(elf,i,&im));
 fixture();elf[5]=2;reject(); fixture();put(16,3,2);reject();fixture();put(18,3,2);reject();fixture();put(32,UINT64_MAX,8);reject();
 fixture();put(56,0xffff,2);reject();fixture();put(68,7,4);reject();fixture();put(88,0x11000,8);reject();fixture();put(104,0,8);reject();fixture();put(96,9000,8);reject();fixture();put(72,UINT64_MAX,8);reject();fixture();put(104,UINT64_MAX,8);reject();fixture();put(112,3,8);reject();fixture();put(72,4097,8);reject();fixture();put(24,0x12000,8);reject();
 fixture();put(56,2,2);memcpy(elf+120,elf+64,56);reject();
 fixture();put(56,2,2);put(120,0x6474e551,4);put(124,6,4);assert(sg_image_inspect(elf,sizeof elf,&im));put(124,7,4);reject();
 fixture();put(56,2,2);put(120,2,4);reject();
 fixture();put(60,1,2);put(40,UINT64_MAX,8);reject();
 fixture();assert(!sg_image_inspect(elf,SG_MAX_IMAGE+1u,&im));
 fixture();put(80,SG_RAM_SIZE-4096,8);put(88,SG_RAM_SIZE-4096,8);reject();
 fixture();put(80,SG_IMAGE_MIN-4096,8);put(88,SG_IMAGE_MIN-4096,8);reject();
 /* Deterministic malformed-header fuzzing under sanitizers. */
 unsigned seed=12345;
 for(unsigned n=0;n<5000;++n) {
  fixture();for(unsigned j=0;j<8;++j){seed=seed*1664525u+1013904223u;elf[seed%256]^=(unsigned char)(seed>>24);}
  struct sg_image before;memset(&before,0x5a,sizeof before);im=before;
  if(!sg_image_inspect(elf,sizeof elf,&im))assert(!memcmp(&im,&before,sizeof im));
 }
 puts("simple guest image: PASS");return 0;
}
