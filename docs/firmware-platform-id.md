# Virtual platform identity, not host microcode access

Observed RDMSR ECX17 at RAM RIP7F6255C. Matching EDK22025.02 has two
consumers: MpInitLib/MpLib.c:637 records PlatformId for a non-AMD CPU;
MicrocodeLib.c:51 obtains it for microcode matching. Both extract bits52:50.
The relocated binary address is not yet symbol-resolved, so these are source
consumers, not a claimed exact stack trace. Candidate logs actual owned bytes
at the exit and requires RDMSR 0F32 before accepting the operation.

The existing guest CPUID is synthetic GenuineIntel, signature00000600,
one logical CPU. The proposed readonly virtual IA32_PLATFORM_ID is class0,
all other bitszero. It names the virtual CPU platform, not the physical host
processor, supported voltage/frequency or microcode revision. Admission checks
that exact vendor/signature. No host MSR read or microcode blob is provided.

MicrocodeDetect returns immediately when its patch-region size iszero
(MpInitLib/Microcode.c:33); neither that size nor the actual binary branch is
assumed here. IA32_BIOS_UPDT_TRIG79, BIOS_SIGN_ID8B and any unknown MSR remain
unsupported fail-stop. WRMSR17 also fails; returning the platform identity
must not imply support for applying microcode. If firmware requests another
MSR, record the concrete operation and analyze separately.

Exclusive transaction checks exact opcode/length/reason, CPL0, canonical RIP
and overflow. Native additionally rejects pending/debug events and validates
owned instruction memory. Only after RIP commit succeeds are EAX/EDX replaced
with zeroextended halves. Failure is terminal; a VMCS write failure is not
claimed to roll back hardware. No retries or double RIP increments.

Host model tests and native acceptance are separate; this candidate has not
proved CPU initialization, firmware microcode handling or DXE completion.
