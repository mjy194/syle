#ifndef LINUX_PIC04_DOMAIN_H
#define LINUX_PIC04_DOMAIN_H
#include "pit0.h"
#include "pic_master_04.h"
#include "linux_pic_mask.h"
#include "linux_uart_runtime.h"
/* All mutable device data is committed together. After any value copy call
 * lpd_bind before use: UART callback must target this copy's master.
 * Helpers require single-BSP serialization and no reserved ticket. I/O false
 * may follow a partial VMWRITE: caller must sticky-fail/no-resume. send/recv
 * have no VMCS callback and publish neither result nor domain on failure. */
struct lpd_domain {struct pit0 timer;struct pic_master_04 master;struct lpic_state slave;struct linux_uart_runtime uart;bool failed;};
bool lpd_init(struct lpd_domain*,uint64_t);
void lpd_bind(struct lpd_domain*);
bool lpd_valid(const struct lpd_domain*);
bool lpd_entry_ok(const struct lpd_domain*,uint64_t);
bool lpd_sync(struct lpd_domain*);
bool lpd_tick(struct lpd_domain*,uint64_t);
bool lpd_ports(struct lpd_domain*,uint64_t[15],uint64_t,uint64_t,unsigned,uint64_t,void*,bool(*)(void*,uint32_t,uint64_t));
bool lpd_pit_io(struct lpd_domain*,uint64_t[15],uint64_t,uint64_t,unsigned,uint64_t,void*,bool(*)(void*,uint32_t,uint64_t));
bool lpd_uart_io(struct lpd_domain*,uint64_t[15],uint64_t,uint64_t,unsigned,void*,bool(*)(void*,uint32_t,uint64_t));
bool lpd_send(struct lpd_domain*,uint8_t,bool*);
bool lpd_recv(struct lpd_domain*,bool*,uint8_t*);
#endif
