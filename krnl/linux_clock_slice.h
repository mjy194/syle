#ifndef SYLE_LINUX_CLOCK_SLICE_H
#define SYLE_LINUX_CLOCK_SLICE_H
/* Default production profile unchanged; experimental targets only reduce
 * the hard per-entry bound. Never allow an experiment to lengthen it. */
#ifndef LGS_PREEMPT_BITS
#define LGS_PREEMPT_BITS 28
#endif
#if LGS_PREEMPT_BITS < 16 || LGS_PREEMPT_BITS > 28
#error "preemption experiment must be within 2^16..2^28 host TSC ticks"
#endif
#endif
