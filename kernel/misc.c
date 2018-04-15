
#include <common/basic_defs.h>
#include <common/string_util.h>

#include <exos/process.h>
#include <exos/hal.h>

uptr ramdisk_paddr = RAMDISK_PADDR; /* default value in case of no multiboot */
size_t ramdisk_size = RAMDISK_SIZE; /* default value in case of no multiboot */

char symtab_buf[16*KB] __attribute__ ((section (".Symtab"))) = {0};
char strtab_buf[16*KB] __attribute__ ((section (".Strtab"))) = {0};

#ifdef DEBUG

void validate_stack_pointer_int(const char *file, int line)
{
   uptr stack_var = 123;
   const uptr stack_var_page = (uptr)&stack_var & PAGE_MASK;

   if (stack_var_page == (uptr)&kernel_initial_stack) {

      /*
       * That's fine: we are in the initialization or in task_switch() called
       * by sys_exit().
       */
      return;
   }

   if (stack_var_page != (uptr)get_current_task()->kernel_stack) {

      disable_interrupts_forced();

      panic("Invalid kernel stack pointer.\n"
            "File %s at line %i\n"
            "[validate stack] stack page: %p\n"
            "[validate stack] expected:   %p\n",
            file, line,
            ((uptr)&stack_var & PAGE_MASK),
            get_current_task()->kernel_stack);
   }
}

#endif
