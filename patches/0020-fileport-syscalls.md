# Patch: fileport_makeport/fileport_makefd syscall wrappers

**Why.** Phase 25's real `config.defs` `notifyviafd` routine passes a
`fileport` argument — a Mach port wrapping a file descriptor, so a
client can hand `configd` an fd it should signal whenever a watched key
changes. Real Darwin does this fd-over-Mach-IPC handoff via
`fileport_makeport()`/`fileport_makefd()`.

**What.** Real syscalls, numbers 430/431, already present in this
project's `sys/syscall.h` but never wrapped in `userland/libc/src/
syscalls.c` — ground-truthed against `src/xnu/bsd/kern/
syscalls.master:665-666`. New `userland/libc/include/sys/fileport.h`
(matches real Darwin's own header, just the two declarations) plus two
one-line wrappers in `syscalls.c`, same shape and same file as every
other less-common syscall there (`poll`, `truncate`, `unlinkat`, ...).
