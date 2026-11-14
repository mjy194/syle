#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <elf.h>
#include "../boot/kernel_elf.h"
int main(int argc,char**argv){assert(argc==2);FILE*f=fopen(argv[1],"rb");assert(f);fseek(f,0,SEEK_END);size_t n=ftell(f);rewind(f);unsigned char*b=malloc(n);assert(fread(b,1,n,f)==n);fclose(f);struct syle_kernel_plan p={0},q={0};assert(!syle_kernel_elf_inspect(b,n,&p));unsigned char*d=malloc(p.size+64);memset(d,0xa5,p.size+64);assert(!syle_kernel_elf_load(b,n,d+32,p.size,&q));assert(!memcmp(&p,&q,sizeof p));for(int i=0;i<32;i++)assert(d[i]==0xa5&&d[p.size+32+i]==0xa5);Elf64_Ehdr*h=(void*)b;Elf64_Phdr*ph=(void*)(b+h->e_phoff);size_t zero=0;for(unsigned i=0;i<h->e_phnum;i++)if(ph[i].p_type==PT_LOAD){assert(!memcmp(d+32+ph[i].p_vaddr-p.base,b+ph[i].p_offset,ph[i].p_filesz));for(size_t j=ph[i].p_filesz;j<ph[i].p_memsz;j++){assert(d[32+ph[i].p_vaddr-p.base+j]==0);zero++;}}printf("loader inspect/load PASS file=%zu span=%llu zero_fill=%zu guards_intact\n",n,(unsigned long long)p.size,zero);free(d);free(b);}
