/* util.h - Utility functions for GnuPG
 * Copyright (C) 2001, 2002, 2003, 2004 Free Software Foundation, Inc.
 *
 * This file is part of GnuPG.
 *
 * GnuPG is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * GnuPG is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
 * USA.
 */

#ifndef GNUPG_COMMON_UTIL_H
#define GNUPG_COMMON_UTIL_H

#include <gcrypt.h> /* We need this for the memory function protos. */
#include <time.h>   /* We need time_t. */
#include <errno.h>  /* We need errno.  */
#include <gpg-error.h> /* We need gpg_error_t. */

/* Common GNUlib includes (-I ../gl/). */
//#include "vasprintf.h"
/* FIXME, moritz, does Poldi need this? */


/* Hash function used with libksba. */
#define HASH_FNC ((void (*)(void *, const void*,size_t))gcry_md_write)

/* Get all the stuff from jnlib. */
#include "../jnlib/logging.h"
#include "../jnlib/argparse.h"
#include "../jnlib/stringhelp.h"
#include "../jnlib/mischelp.h"
#include "../jnlib/strlist.h"
#include "../jnlib/dotlock.h"
#include "../jnlib/utf8conv.h"


#if __GNUC__ >= 4 
# define GNUPG_GCC_A_SENTINEL(a) __attribute__ ((sentinel(a)))
#else
# define GNUPG_GCC_A_SENTINEL(a) 
#endif


/* We need this type even if we are not using libreadline and or we
   did not include libreadline in the current file. */
#ifndef GNUPG_LIBREADLINE_H_INCLUDED
typedef char **rl_completion_func_t (const char *, int, int);
#endif /*!GNUPG_LIBREADLINE_H_INCLUDED*/


/* Handy malloc macros - please use only them. */
#define xtrymalloc(a)    gcry_malloc ((a))
#define xtrymalloc_secure(a)  gcry_malloc_secure ((a))
#define xtrycalloc(a,b)  gcry_calloc ((a),(b))
#define xtrycalloc_secure(a,b)  gcry_calloc_secure ((a),(b))
#define xtryrealloc(a,b) gcry_realloc ((a),(b))
#define xtrystrdup(a)    gcry_strdup ((a))
#define xfree(a)         gcry_free ((a))

#define xmalloc(a)       gcry_xmalloc ((a))
#define xmalloc_secure(a)  gcry_xmalloc_secure ((a))
#define xcalloc(a,b)     gcry_xcalloc ((a),(b))
#define xcalloc_secure(a,b) gcry_xcalloc_secure ((a),(b))
#define xrealloc(a,b)    gcry_xrealloc ((a),(b))
#define xstrdup(a)       gcry_xstrdup ((a))

/* For compatibility with gpg 1.4 we also define these: */
#define xmalloc_clear(a) gcry_xcalloc (1, (a))
#define xmalloc_secure_clear(a) gcry_xcalloc_secure (1, (a))

/* Convenience function to return a gpg-error code for memory
   allocation failures.  This function makes sure that an error will
   be returned even if accidently ERRNO is not set.  */
static inline gpg_error_t
out_of_core (void)
{
  return gpg_error_from_syserror ();
}

/* A type to hold the ISO time.  Note that this this is the same as
   the the KSBA type ksba_isotime_t. */
typedef char gnupg_isotime_t[16];


/*-- gettime.c --*/
time_t gnupg_get_time (void);
void   gnupg_get_isotime (gnupg_isotime_t timebuf);
void   gnupg_set_time (time_t newtime, int freeze);
int    gnupg_faked_time_p (void);
u32    make_timestamp (void);
u32    scan_isodatestr (const char *string);
u32    add_days_to_timestamp (u32 stamp, u16 days);
const char *strtimevalue (u32 stamp);
const char *strtimestamp (u32 stamp); /* GMT */
const char *isotimestamp (u32 stamp); /* GMT */
const char *asctimestamp (u32 stamp); /* localized */


/* Copy one iso ddate to another, this is inline so that we can do a
   sanity check. */
static inline void
gnupg_copy_time (gnupg_isotime_t d, const gnupg_isotime_t s)
{
  if (*s && (strlen (s) != 15 || s[8] != 'T'))
    BUG();
  strcpy (d, s);
}


/*-- signal.c --*/
void gnupg_init_signals (int mode, void (*fast_cleanup)(void));
void gnupg_pause_on_sigusr (int which);
void gnupg_block_all_signals (void);
void gnupg_unblock_all_signals (void);

/*-- yesno.c --*/
int answer_is_yes (const char *s);
int answer_is_yes_no_default (const char *s, int def_answer);
int answer_is_yes_no_quit (const char *s);
int answer_is_okay_cancel (const char *s, int def_answer);

/*-- xreadline.c --*/
ssize_t read_line (FILE *fp, 
                   char **addr_of_buffer, size_t *length_of_buffer,
                   size_t *max_length);


/*-- b64enc.c --*/
struct b64state 
{ 
  unsigned int flags;
  int idx;
  int quad_count;
  FILE *fp;
  char *title;
  unsigned char radbuf[4];
};
gpg_error_t b64enc_start (struct b64state *state, FILE *fp, const char *title);
gpg_error_t b64enc_write (struct b64state *state,
                          const void *buffer, size_t nbytes);
gpg_error_t b64enc_finish (struct b64state *state);

/*-- sexputil.c */
gpg_error_t keygrip_from_canon_sexp (const unsigned char *key, size_t keylen,
                                     unsigned char *grip);
int cmp_simple_canon_sexp (const unsigned char *a, const unsigned char *b);
unsigned char *make_simple_sexp_from_hexstr (const char *line,
                                             size_t *nscanned);

/*-- convert.c --*/
int hex2bin (const char *string, void *buffer, size_t length);
int hexcolon2bin (const char *string, void *buffer, size_t length);
char *bin2hex (const void *buffer, size_t length, char *stringbuf);
char *bin2hexcolon (const void *buffer, size_t length, char *stringbuf);


/*-- homedir.c --*/
const char *default_homedir (void);

/*-- gpgrlhelp.c --*/
void gnupg_rl_initialize (void);

/*-- miscellaneous.c --*/

/* Same as asprintf but return an allocated buffer suitable to be
   freed using xfree.  This function simply dies on memory failure,
   thus no extra check is required. */
char *xasprintf (const char *fmt, ...) JNLIB_GCC_A_PRINTF(1,2);
/* Same as asprintf but return an allocated buffer suitable to be
   freed using xfree.  This function returns NULL on memory failure and
   sets errno. */
char *xtryasprintf (const char *fmt, ...) JNLIB_GCC_A_PRINTF(1,2);

const char *print_fname_stdout (const char *s);
const char *print_fname_stdin (const char *s);
void print_string (FILE *fp, const byte *p, size_t n, int delim);
void print_utf8_string2 ( FILE *fp, const byte *p, size_t n, int delim);
void print_utf8_string (FILE *fp, const byte *p, size_t n);
char *make_printable_string (const void *p, size_t n, int delim);

int is_file_compressed (const char *s, int *ret_rc);

int match_multistr (const char *multistr,const char *match);


#if 0
/* FIXME, moritz, weird error and maybe simply not necessary for
   Poldi... (?).  */

/*-- Simple replacement functions. */
#ifndef HAVE_TTYNAME
/* Systems without ttyname (W32) will merely return NULL. */
static inline char *
ttyname (int fd) 
{
  return NULL
};
#endif /* !HAVE_TTYNAME */

#endif


/*-- Macros to replace ctype ones to avoid locale problems. --*/
#define spacep(p)   (*(p) == ' ' || *(p) == '\t')
#define digitp(p)   (*(p) >= '0' && *(p) <= '9')
#define hexdigitp(a) (digitp (a)                     \
                      || (*(a) >= 'A' && *(a) <= 'F')  \
                      || (*(a) >= 'a' && *(a) <= 'f'))
  /* Note this isn't identical to a C locale isspace() without \f and
     \v, but works for the purposes used here. */
#define ascii_isspace(a) ((a)==' ' || (a)=='\n' || (a)=='\r' || (a)=='\t')

/* The atoi macros assume that the buffer has only valid digits. */
#define atoi_1(p)   (*(p) - '0' )
#define atoi_2(p)   ((atoi_1(p) * 10) + atoi_1((p)+1))
#define atoi_4(p)   ((atoi_2(p) * 100) + atoi_2((p)+2))
#define xtoi_1(p)   (*(p) <= '9'? (*(p)- '0'): \
                     *(p) <= 'F'? (*(p)-'A'+10):(*(p)-'a'+10))
#define xtoi_2(p)   ((xtoi_1(p) * 16) + xtoi_1((p)+1))
#define xtoi_4(p)   ((xtoi_2(p) * 256) + xtoi_2((p)+2))



#endif /*GNUPG_COMMON_UTIL_H*/
