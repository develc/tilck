/* SPDX-License-Identifier: BSD-2-Clause */

#pragma once
#include <tilck/common/basic_defs.h>

enum term_action {

   a_none,
   a_write,
   a_dwrite_no_filter,   /* direct write w/o filters/scroll/move_cursor/flush */
   a_del,
   a_scroll,               /* > 0 scrollup: text moves DOWN; < 0 the opposite */
   a_set_col_offset,
   a_move_ch_and_cur,
   a_move_ch_and_cur_rel,
   a_reset,
   a_erase_in_display,
   a_erase_in_line,
   a_non_buf_scroll_up,   /* text moves up => new blank lines at the bottom  */
   a_non_buf_scroll_down, /* text moves down => new blank lines at the top   */
   a_pause_video_output,
   a_restart_video_output

};

typedef void (*action_func)(term *t, ...);

typedef struct {

   action_func func;
   u32 args_count;

} actions_table_item;

enum term_del_type {

   TERM_DEL_PREV_CHAR,
   TERM_DEL_PREV_WORD
};

/* --- term write filter interface --- */

enum term_fret {
   TERM_FILTER_WRITE_BLANK,
   TERM_FILTER_WRITE_C
};

typedef struct {

   union {

      struct {
         u32 type3 :  4;
         u32 len   : 20;
         u32 col   :  8;
      };

      struct {
         u32 type2 :  4;
         u32 arg1  : 14;
         u32 arg2  : 14;
      };

      struct {
         u32 type1  :  4;
         u32 arg    : 28;
      };

   };

   uptr ptr;

} term_action;

STATIC_ASSERT(sizeof(term_action) == (2 * sizeof(uptr)));

typedef enum term_fret (*term_filter)(u8 *c,          /* in/out */
                                      u8 *color,      /* in/out */
                                      term_action *a, /*  out   */
                                      void *ctx);     /*   in   */

void term_set_filter(term *t, term_filter func, void *ctx);
term_filter term_get_filter(term *t);

/* --- */

term *alloc_term_struct(void);
void free_term_struct(term *t);
void dispose_term(term *t);

void set_curr_term(term *t);