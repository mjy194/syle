#include "linux_guest_session.h"
#include "managed_host_context.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned reset_calls,enters,windows,leaves,backend_calls,reads,writes;
static bool allow_reset,uart_ready;
static int backend_prepare(void*c,uint64_t p){(void)c;(void)p;backend_calls++;return 0;}
const struct guest_backend lgs_guest_backend={.prepare=backend_prepare};
static char marker[64];
static struct {struct {bool(*reset)(void*);void*ctx;}service;} platform;
static bool reset(void*x){(void)x;reset_calls++;return allow_reset;}
static void log_text(const char*s){(void)s;}
static struct mhc_saved enter_context(void){enters++;return(struct mhc_saved){.entered=true};}
static void leave_context(struct mhc_saved*s){assert(s->entered&&windows==enters);s->entered=false;leaves++;}
unsigned char observe_test_in(uint16_t p){assert(p==0x3fd);reads++;return uart_ready?0x20:0;}
void observe_test_out(uint16_t p,unsigned char b){assert(p==0x3f8&&writes<sizeof marker);marker[writes++]=(char)b;}
void source_prepare_pause(void){assert(!strcmp(marker,"SOURCE.PREPARE_CONTEXT_WINDOW\r\n"));}
extern void actual_source_prepare_observe(void);
static void source_prepare_observe(void){windows++;actual_source_prepare_observe();}
#include "prepare_fragment.inc"
int main(void){
 static struct lgs_session session;platform.service.reset=reset;
 assert(prepare_context_candidate(&session,2)&&!reset_calls&&!enters);
 session.prepared=true;assert(prepare_context_candidate(&session,1)&&!reset_calls&&!enters);session.prepared=false;
 assert(prepare_context_candidate(&session,1)&&reset_calls==1&&!enters);
 allow_reset=uart_ready=true;assert(prepare_context_candidate(&session,1)&&enters==1&&windows==1&&leaves==1&&!backend_calls);
 assert(!session.prepared&&!session.enabled&&!session.host.active&&!session.memory.eptp);
 reads=writes=0;uart_ready=false;assert(prepare_context_candidate(&session,1)&&reads==10000&&!writes&&enters==2&&leaves==2&&!backend_calls);
 puts("isolated actual prepare fragment: rejection/window/restore/failure; no backend call PASS");return 0;
}
