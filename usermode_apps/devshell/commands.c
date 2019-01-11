/* SPDX-License-Identifier: BSD-2-Clause */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/mman.h>

#include "devshell.h"
#include "sysenter.h"

int cmd_brk_test(int argc, char **argv);
int cmd_mmap_test(int argc, char **argv);
int cmd_waitpid1(int argc, char **argv);
int cmd_waitpid2(int argc, char **argv);
int cmd_waitpid3(int argc, char **argv);
int cmd_waitpid4(int argc, char **argv);
int cmd_waitpid5(int argc, char **argv);
int cmd_fork_test(int argc, char **argv);
int cmd_fork_perf(int argc, char **argv);
int cmd_se_fork_test(int argc, char **argv);

int cmd_loop(int argc, char **argv)
{
   printf("[shell] do a long loop\n");
   for (int i = 0; i < (2 * 1000 * 1000 * 1000); i++) {
      __asm__ volatile ("nop");
   }

   return 0;
}

int cmd_bad_read(int argc, char **argv)
{
   int ret;
   void *addr = (void *) 0xB0000000;
   printf("[cmd] req. kernel to read unaccessibile user addr: %p\n", addr);

   /* write to stdout a buffer unaccessibile for the user */
   errno = 0;
   ret = write(1, addr, 16);
   printf("ret: %i, errno: %i: %s\n", ret, errno, strerror(errno));

   addr = (void *) 0xC0000000;
   printf("[cmd] req. kernel to read unaccessible user addr: %p\n", addr);

   /* write to stdout a buffer unaccessibile for the user */
   errno = 0;
   ret = write(1, addr, 16);
   printf("ret: %i, errno: %i: %s\n", ret, errno, strerror(errno));

   printf("Open with filename invalid ptr\n");
   ret = open((char*)0xB0000000, 0);

   printf("ret: %i, errno: %i: %s\n", ret, errno, strerror(errno));
   return 0;
}

int cmd_bad_write(int argc, char **argv)
{
   int ret;
   void *addr = (void *) 0xB0000000;

   errno = 0;
   ret = stat("/", addr);
   printf("ret: %i, errno: %i: %s\n", ret, errno, strerror(errno));
   return 0;
}

int cmd_sysenter(int argc, char **argv)
{
   const char *str = "hello from a sysenter call!\n";
   size_t len = strlen(str);

   int ret = sysenter_call3(4  /* write */,
                            1  /* stdout */,
                            str,
                            len);

   printf("The syscall returned: %i\n", ret);
   printf("sleep (int 0x80)..\n");
   usleep(100*1000);
   printf("after sleep, everything is fine.\n");
   printf("same sleep, but with sysenter:\n");

   struct timespec req = { .tv_sec = 0, .tv_nsec = 100*1000*1000 };
   sysenter_call3(162 /* nanosleep */, &req, NULL, NULL);
   printf("after sleep, everything is fine. Prev ret: %i\n", ret);
   return 0;
}

int cmd_syscall_perf(int argc, char **argv)
{
   const int iters = 1000;
   unsigned long long start, duration;
   pid_t uid = getuid();

   start = RDTSC();

   for (int i = 0; i < iters; i++) {
      __asm__ ("int $0x80"
               : /* no output */
               : "a" (23),          /* 23 = sys_setuid */
                 "b" (uid)
               : /* no clobber */);
   }

   duration = RDTSC() - start;

   printf("int 0x80 setuid(): %llu cycles\n", duration/iters);

   start = RDTSC();

   for (int i = 0; i < iters; i++)
      sysenter_call1(23 /* setuid */, uid /* uid */);

   duration = RDTSC() - start;

   printf("sysenter setuid(): %llu cycles\n", duration/iters);
   return 0;
}

int cmd_fpu(int argc, char **argv)
{
   long double e = 1.0;
   long double f = 1.0;

   for (unsigned i = 1; i < 40; i++) {
      f *= i;
      e += (1.0 / f);
   }

   printf("e(1): %.10Lf\n", e);
   return 0;
}

int cmd_fpu_loop(int argc, char **argv)
{
   register double num = 0;

   for (unsigned i = 0; i < 1000*1000*1000; i++) {

      if (!(i % 1000000))
         printf("%f\n", num);

      num += 1e-6;
   }

   return 0;
}

int cmd_kernel_cow(int argc, char **argv)
{
   static char cow_buf[4096];

   int wstatus;
   int child_pid = fork();

   if (child_pid < 0) {
      printf("fork() failed\n");
      return 1;
   }

   if (!child_pid) {
      int rc = stat("/", (void *)cow_buf);
      printf("stat() returned: %d (errno: %s)\n", rc, strerror(errno));
      exit(0); // exit from the child
   }

   waitpid(child_pid, &wstatus, 0);
   return 0;
}

int cmd_selftest(int argc, char **argv)
{
   if (argc < 1) {
      printf("[shell] Expected selftest name argument.\n");
      return 1;
   }

   int rc =
      sysenter_call3(TILCK_TESTCMD_SYSCALL,
                     TILCK_TESTCMD_RUN_SELFTEST,
                     argv[0]  /* self test name */,
                     NULL);

   if (rc != 0) {
      printf("[shell] Invalid selftest '%s'\n", argv[0]);
      return 1;
   }

   return 0;
}

int cmd_help(int argc, char **argv);

/* ------------------------------------------- */

enum timeout_type {
   TT_SHORT = 0,
   TT_MED   = 1,
   TT_LONG  = 2,
};

static const char *tt_str[] =
{
   [TT_SHORT] = "tt_short",
   [TT_MED] = "tt_med",
   [TT_LONG] = "tt_long"
};

struct {

   const char *name;
   cmd_func_type fun;
   enum timeout_type tt;
   bool enabled_in_st;

} cmds_table[] = {

   {"help", cmd_help, TT_SHORT, false},
   {"selftest", cmd_selftest, TT_LONG, false},
   {"loop", cmd_loop, TT_MED, false},
   {"fork_test", cmd_fork_test, TT_MED, true},
   {"bad_read", cmd_bad_read, TT_SHORT, true},
   {"bad_write", cmd_bad_write, TT_SHORT, true},
   {"fork_perf", cmd_fork_perf, TT_LONG, true},
   {"sysenter", cmd_sysenter, TT_SHORT, true},
   {"syscall_perf", cmd_syscall_perf, TT_SHORT, true},
   {"se_fork_test", cmd_se_fork_test, TT_MED, true},
   {"fpu", cmd_fpu, TT_SHORT, true},
   {"fpu_loop", cmd_fpu_loop, TT_LONG, false},
   {"brk_test", cmd_brk_test, TT_SHORT, true},
   {"mmap_test", cmd_mmap_test, TT_MED, true},
   {"kernel_cow", cmd_kernel_cow, TT_SHORT, true},
   {"waitpid1", cmd_waitpid1, TT_SHORT, true},
   {"waitpid2", cmd_waitpid2, TT_SHORT, true},
   {"waitpid3", cmd_waitpid3, TT_SHORT, true},
   {"waitpid4", cmd_waitpid4, TT_SHORT, true},
   {"waitpid5", cmd_waitpid5, TT_SHORT, true}
};

void dump_list_of_commands(void)
{
   for (int i = 1; i < ARRAY_SIZE(cmds_table); i++) {
      if (cmds_table[i].enabled_in_st)
         printf("%s %s\n", cmds_table[i].name, tt_str[cmds_table[i].tt]);
   }

   exit(0);
}

int cmd_help(int argc, char **argv)
{
   printf("\n");
   printf(COLOR_RED "Tilck development shell\n" RESET_ATTRS);

   printf("This application is a small dev-only utility written in ");
   printf("order to allow running\nsimple programs, while proper shells ");
   printf("like ASH can't run on Tilck yet. Behavior:\nif a given command ");
   printf("isn't an executable (e.g. /bin/termtest), it is forwarded ");
   printf("to\n" COLOR_YELLOW "/bin/busybox" RESET_ATTRS);
   printf(". That's how several programs like 'ls' work. Type --help to see\n");
   printf("all the commands built in busybox.\n\n");

   printf(COLOR_RED "Built-in commands\n" RESET_ATTRS);
   printf("    help: shows this help\n");
   printf("    cd <directory>: change the current working directory\n\n");
   printf(COLOR_RED "Kernel tests\n" RESET_ATTRS);

   const int elems_per_row = 7;

   for (int i = 1 /* skip help */; i < ARRAY_SIZE(cmds_table); i++) {
      printf("%s%s%s ",
             (i % elems_per_row) != 1 ? "" : "    ",
             cmds_table[i].name,
             i != ARRAY_SIZE(cmds_table)-1 ? "," : "");

      if (!(i % elems_per_row))
         printf("\n");
   }

   printf("\n\n");
   return 0;
}

static void run_cmd(cmd_func_type func, int argc, char **argv)
{
   int exit_code = func(argc, argv);

   if (dump_coverage) {
      dump_coverage_files();
   }

   exit(exit_code);
}

void run_if_known_command(const char *cmd, int argc, char **argv)
{
   for (int i = 0; i < ARRAY_SIZE(cmds_table); i++)
      if (!strcmp(cmds_table[i].name, cmd))
         run_cmd(cmds_table[i].fun, argc, argv);
}