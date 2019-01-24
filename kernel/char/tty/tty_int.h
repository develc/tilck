/* SPDX-License-Identifier: BSD-2-Clause */

#pragma once
#include <tilck/common/basic_defs.h>
#include <tilck/kernel/tty.h>
#include <tilck/kernel/ringbuf.h>
#include <tilck/kernel/sync.h>

enum term_write_filter_state {

   TERM_WFILTER_STATE_DEFAULT,
   TERM_WFILTER_STATE_ESC1,
   TERM_WFILTER_STATE_ESC2_CSI,
   TERM_WFILTER_STATE_ESC2_PAR,
   TERM_WFILTER_STATE_ESC2_UNKNOWN

};

typedef struct {

   tty *t;

   enum term_write_filter_state state;
   char param_bytes[64];
   char interm_bytes[64];

   bool use_alt_charset;

   u8 pbc; /* param bytes count */
   u8 ibc; /* intermediate bytes count */

} term_write_filter_ctx_t;

void tty_input_init(tty *t);
int tty_keypress_handler(u32 key, u8 c);

enum term_fret
tty_term_write_filter(u8 c, u8 *color, term_action *a, void *ctx_arg);

typedef bool (*tty_ctrl_sig_func)(tty *);

#define KB_INPUT_BS 4096
#define MAX_TTYS 3

struct tty {

   char kb_input_buf[KB_INPUT_BS];
   ringbuf kb_input_ringbuf;

};

extern struct termios c_term;
extern const struct termios default_termios;
extern u32 tty_kd_mode;

extern term_write_filter_ctx_t term_write_filter_ctx;
extern u8 tty_curr_color;

extern kcond kb_input_cond;
extern volatile int tty_end_line_delim_count;
extern tty_ctrl_sig_func tty_special_ctrl_handlers[256];

extern u16 tty_saved_cursor_row;
extern u16 tty_saved_cursor_col;
extern term_write_filter_ctx_t term_write_filter_ctx;

//------
extern tty *ttys[MAX_TTYS];
