/* poldi.c - PAM authentication via OpenPGP smartcards.
   Copyright (C) 2004, 2007 g10 Code GmbH.
 
   This file is part of Libgcrypt.
  
   Libgcrypt is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser general Public License as
   published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.
  
   Libgcrypt is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.
  
   You should have received a copy of the GNU Lesser General Public
   License along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#include <config.h>

#include <stdio.h>
#include <errno.h>

#include <gpg-error.h>

#include "../jnlib/argparse.h"
#include "../jnlib/xmalloc.h"

#include "optparse.h"

#include <defs.h>



gpg_error_t
options_parse_argv (options_callback_t callback, void *opaque,
		    ARGPARSE_OPTS *arg_opts, int argc, char **argv)
{
  ARGPARSE_ARGS pargs;
  gpg_error_t err;

  err = 0;
  pargs.argc = &argc;
  pargs.argv = &argv;
  pargs.flags = 1;
  while (arg_parse (&pargs, arg_opts))
    {
      err = (*callback) (&pargs, opaque);
      if (err)
	break;
      }

  return err;
}

gpg_error_t
options_parse_conf (options_callback_t callback, void *opaque,
		    ARGPARSE_OPTS *arg_opts, const char *filename)
{
  unsigned int line_no;
  ARGPARSE_ARGS pargs;
  FILE *filename_fp;
  gpg_error_t err;
  char **argv;
  int argc;
  
  filename_fp = fopen (filename, "r");
  if (! filename_fp)
    {
      if (errno == ENOENT)
	err = 0;
      else
	err = gpg_error_from_errno (errno);
      goto out;
    }

  argc = 0;
  argv = NULL;
  pargs.argc = &argc;
  pargs.argv = &argv;
  pargs.flags = 0;

  err = 0;
  line_no = 0;
  while (optfile_parse (filename_fp, filename, &line_no, &pargs, arg_opts))
    err = (*callback) (&pargs, opaque);

 out:

  if (filename_fp)
    fclose (filename_fp);

  return err;
}
