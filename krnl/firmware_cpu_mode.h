#ifndef SYLE_FIRMWARE_CPU_MODE_H
#define SYLE_FIRMWARE_CPU_MODE_H
#include <stdbool.h>
#include <stdint.h>
/* Pure restricted tiny-ROM mode-transition model, not a general x86 emulator.
 * Guest read shadows only; caller must apply hardware fixed bits, update VMCS
 * EFER.LMA and VM-entry IA32e together, and fail-stop on partial commit.
 */
struct fcm_state { uint64_t cr0,cr3,cr4,efer; bool ia32e; };
enum fcm_register { FCM_CR0,FCM_CR3,FCM_CR4,FCM_EFER };
bool fcm_write(const struct fcm_state*,uint64_t ram_size,enum fcm_register,uint64_t,struct fcm_state*);
#endif
