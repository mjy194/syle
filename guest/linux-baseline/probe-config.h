#ifndef SYLE_BASELINE_PROBE_CONFIG_H
#define SYLE_BASELINE_PROBE_CONFIG_H
/* Command echo cannot match the independent final result line. Each operation
 * must succeed; exit37 is deliberately checked rather than treated as failure. */
#define MG_SHELL_COMMAND "/bin/baseline-probe check && ( /bin/baseline-probe exit37; test \"$?\" -eq 37 ) && /bin/sleep 1 && echo SYLE_\"\"BASELINE_OK\n"
#define MG_SHELL_REPLY "SYLE_BASELINE_OK"
#endif
