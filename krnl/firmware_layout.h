#ifndef SYLE_FIRMWARE_LAYOUT_H
#define SYLE_FIRMWARE_LAYOUT_H
#include <stdbool.h>
#include <stdint.h>

/* Pure address plan only. No allocation, mapping, VMX or firmware execution.
 * GPA permissions describe the intended backing. VARS writes must be trapped
 * for flash emulation, not silently treated as ordinary writable RAM.
 */
#define FWL_PAGE UINT64_C(4096)
#define FWL_TOP UINT64_C(0x100000000)
#define FWL_RESET UINT64_C(0xfffffff0)
#define FWL_LOW_MMIO UINT64_C(0xc0000000)
enum fwl_kind { FWL_RAM, FWL_FLASH_VARS, FWL_ROM_CODE };
struct fwl_region { uint64_t base,size; enum fwl_kind kind; bool read,write,execute; };
struct fwl_request { uint64_t ram_size,code_size,vars_size; unsigned physical_bits; };
struct fwl_plan {
 struct fwl_region ram,vars,code;
 uint64_t reset_linear,reset_code_offset;
};
/* One low contiguous RAM range and contiguous flash ending at 4GiB.
 * The [ram.end, vars.base) hole remains unmapped; future MMIO is separate.
 * Input and output must not overlap. Output is unchanged on rejection.
 * This does not establish host backing ownership or firmware compatibility.
 */
bool fwl_plan_build(const struct fwl_request*,struct fwl_plan*);
bool fwl_contains(const struct fwl_region*,uint64_t address,uint64_t size);
#endif
