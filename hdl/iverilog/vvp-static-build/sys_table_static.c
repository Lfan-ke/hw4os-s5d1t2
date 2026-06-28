/* Minimal system VPI startup table for the StarryOS static vvp (#764 verilog).
   Text-I/O system tasks only ($display/$write/$monitor/$finish/$time/$random/
   $readmem/...), NO waveform modules (VCD/LXT/FST) — avoids zlib/bzip2/nexus/
   threading deps that can't link into a static musl binary. */
extern void sys_convert_register(void);
extern void sys_countdrivers_register(void);
extern void sys_darray_register(void);
extern void sys_deposit_register(void);
extern void sys_display_register(void);
extern void sys_fileio_register(void);
extern void sys_finish_register(void);
extern void sys_plusargs_register(void);
extern void sys_queue_register(void);
extern void sys_random_register(void);
extern void sys_random_mti_register(void);
extern void sys_readmem_register(void);
extern void sys_scanf_register(void);
extern void sys_time_register(void);
void (*vlog_startup_routines[])(void) = {
      sys_convert_register,    sys_countdrivers_register, sys_darray_register,
      sys_deposit_register,    sys_display_register,      sys_fileio_register,
      sys_finish_register,     sys_plusargs_register,     sys_queue_register,
      sys_random_register,     sys_random_mti_register,   sys_readmem_register,
      sys_scanf_register,      sys_time_register,         0
};
