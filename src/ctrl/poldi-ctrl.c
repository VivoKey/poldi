/* poldi-ctrl.c - Poldi maintaince tool
   Copyright (C) 2004 Free Software Foundation, Inc.
 
   This file is part of Poldi.
  
   Poldi is free software; you can redistribute it and/or modify it
   under the terms of the GNU general Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
  
   Poldi is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.
  
   You should have received a copy of the GNU Lesser General Public
   License along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#include <config.h>

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pwd.h>

#include <gcrypt.h>

#include <jnlib/argparse.h>
#include <jnlib/xmalloc.h>
#include <jnlib/stringhelp.h>
#include <jnlib/logging.h>
#include <common/options.h>
#include <common/card.h>
#include <common/support.h>
#include <common/defs.h>
#include <libscd/scd.h>

/* Global flags.  */
struct poldi_ctrl_opt
{
  unsigned int debug; /* debug flags (DBG_foo_VALUE) */
  int debug_sc;     /* OpenSC debug level */
  int verbose;      /* verbosity level */
  const char *ctapi_driver; /* Library to access the ctAPI. */
  const char *pcsc_driver;  /* Library to access the PC/SC system. */
  const char *reader_port;  /* NULL or reder port to use. */
  int disable_opensc;  /* Disable the use of the OpenSC framework. */
  int disable_ccid;    /* Disable the use of the internal CCID driver. */
  int debug_ccid_driver;	/* Debug the internal CCID driver.  */
  const char *config_file;
  const char *account;
  const char *serialno;
  int fake_wait_for_card;
  int cmd_test;
  int cmd_dump;
  int cmd_set_key;
  int cmd_show_key;
  int cmd_add_user;
  int cmd_remove_user;
  int cmd_list_users;
} poldi_ctrl_opt;

/* Set defaults.  */
struct poldi_ctrl_opt poldi_ctrl_opt =
  {
    0,
    0,
    0,
    NULL,
    NULL,
    NULL,
    0,
    0,
    0,
    POLDI_CONF_FILE,
    NULL,
    NULL,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0
  };

enum arg_opt_ids
  {
    arg_test = 't',
    arg_dump = 'd',
    arg_set_key = 's',
    arg_show_key = 'k',
    arg_add_user  = 'a',
    arg_remove_user  = 'r',
    arg_list_users  = 'l',
    arg_config_file = 'c',
    arg_ctapi_driver = 500,
    arg_account,
    arg_serialno,
    arg_pcsc_driver,
    arg_reader_port,
    arg_disable_ccid,
    arg_disable_opensc,
    arg_debug_ccid_driver,
    arg_fake_wait_for_card,
  };

static ARGPARSE_OPTS arg_opts[] =
  {
    { arg_test,
      "test",        256, "Test authentication"                },
    { arg_dump,
      "dump",        256, "Dump certain card information"      },
    { arg_add_user,
      "add-user",    256, "Add account to users db"            },
    { arg_remove_user,
      "remove-user",    256, "Remove account from users db"    },
    { arg_list_users,
      "list-users",    256, "List accounts from users db"      },
    { arg_set_key,
      "set-key",     256, "Set key for calling user"           },
    { arg_show_key,
      "show-key",     256, "Show key of calling user"          },
    { arg_config_file,
      "config-file",   2, "|FILE|Specify configuration file"   },
    { arg_account,
      "account",       2, "|NAME|Specify Unix account"         },
    { arg_serialno,
      "serialno",      2, "|NAME|Specify card serial number"   },
    { arg_ctapi_driver,
      "ctapi-driver", 2, "|NAME|use NAME as ct-API driver"     },
    { arg_pcsc_driver,
      "pcsc-driver", 2, "|NAME|use NAME as PC/SC driver"       },
    { arg_reader_port,
      "reader-port", 2, "|N|connect to reader at port N"       },
#ifdef HAVE_LIBUSB
    { arg_disable_ccid,
      "disable-ccid", 0, "do not use the internal CCID driver" },
    { arg_debug_ccid_driver,
      "debug-ccid-driver", 0, "debug the  internal CCID driver" },
#endif
#ifdef HAVE_OPENSC
    { arg_disable_opensc,
      "disable-opensc", 0, "do not use the OpenSC layer"       },
#endif
    { arg_fake_wait_for_card,
      "fake-wait-for-card", 0, "Fake wait-for-card feature"    },
    { 0,
      NULL,            0, NULL                                 }
  };

static const char *
my_strusage (int level)
{
  const char *p;

  switch (level)
    {
    case 11:
      p = "poldi-ctrl (Poldi)";
      break;
    case 13:
      p = VERSION;
      break;
    case 17:
      p = "Fixme OS";
      break;
    case 19:
      p = "Please report bugs to FIXME\n";
      break;
    case 1:
    case 40:
      p = "Usage: poldi-ctrl [options] [command]";
      break;
    case 41:
      p = "Syntax: poldi-ctrl [options] [command]\n";
      break;

    default:
      p = NULL;
    }

  return p;
}

static gpg_error_t
poldi_ctrl_options_cb (ARGPARSE_ARGS *parg, void *opaque)
{
  int parsing_stage = *((int *) opaque);
  gpg_err_code_t err = GPG_ERR_NO_ERROR;

  switch (parg->r_opt)
    {
    case arg_config_file:
      if (! parsing_stage)
	poldi_ctrl_opt.config_file = xstrdup (parg->r.ret_str);
      break;

    case arg_test:
      if (parsing_stage)
	poldi_ctrl_opt.cmd_test = 1;
      break;

    case arg_set_key:
      if (parsing_stage)
	poldi_ctrl_opt.cmd_set_key = 1;
      break;

    case arg_show_key:
      if (parsing_stage)
	poldi_ctrl_opt.cmd_show_key = 1;
      break;

    case arg_dump:
      if (parsing_stage)
	poldi_ctrl_opt.cmd_dump = 1;
      break;

    case arg_add_user:
      if (parsing_stage)
	poldi_ctrl_opt.cmd_add_user = 1;
      break;

    case arg_remove_user:
      if (parsing_stage)
	poldi_ctrl_opt.cmd_remove_user = 1;
      break;

    case arg_list_users:
      if (parsing_stage)
	poldi_ctrl_opt.cmd_list_users = 1;
      break;

    case arg_account:
      if (parsing_stage)
	poldi_ctrl_opt.account = xstrdup (parg->r.ret_str);
      break;
      
    case arg_serialno:
      if (parsing_stage)
	poldi_ctrl_opt.serialno = xstrdup (parg->r.ret_str);
      break;
      
    case arg_ctapi_driver:
      if (parsing_stage)
	poldi_ctrl_opt.ctapi_driver = xstrdup (parg->r.ret_str);
      break;

    case arg_pcsc_driver:
      if (parsing_stage)
	poldi_ctrl_opt.pcsc_driver = xstrdup (parg->r.ret_str);
      break;

    case arg_reader_port:
      if (parsing_stage)
	poldi_ctrl_opt.reader_port = xstrdup (parg->r.ret_str);
      break;

    case arg_disable_ccid:
      if (parsing_stage)
	poldi_ctrl_opt.disable_ccid = 1;
      break;

    case arg_disable_opensc:
      if (parsing_stage)
	poldi_ctrl_opt.disable_opensc = 1;
      break;

    case arg_debug_ccid_driver:
      if (parsing_stage)
	poldi_ctrl_opt.debug_ccid_driver = 1;
      break;

    case arg_fake_wait_for_card:
      if (parsing_stage)
	poldi_ctrl_opt.fake_wait_for_card = 1;
      break;

    default:
      err = GPG_ERR_INTERNAL;	/* FIXME?  */
      break;
    }

  return gpg_error (err);
}

static gpg_error_t
cmd_test (void)
{
  unsigned char *challenge;
  unsigned char *signature;
  size_t challenge_n;
  size_t signature_n;
  gpg_error_t err;
  int slot;
  char *serialno;
  char *account;
  char *pin;
  struct passwd *pwent;
  char *key_path;
  gcry_sexp_t key_sexp;
  char *key_string;

  slot = -1;
  pin = NULL;
  key_path = NULL;
  key_sexp = NULL;
  key_string = NULL;
  account = NULL;
  challenge = NULL;
  signature = NULL;
  serialno = NULL;

  err = challenge_generate (&challenge, &challenge_n);
  if (err)
    goto out;

  err = card_open (NULL, &slot);
  if (err)
    goto out;

  if (poldi_ctrl_opt.fake_wait_for_card)
    {
      printf ("Press ENTER when card is available...\n");
      getchar ();
    }
  else
    printf ("Waiting for card...\n");
  err = card_init (slot, !poldi_ctrl_opt.fake_wait_for_card);
  if (err)
    goto out;

  err = card_info (slot, &serialno, NULL);
  if (err)
    goto out;

  printf ("Serial No: %s\n", serialno);

  err = serialno_to_username (serialno, &account);
  if (err)
    goto out;

  printf ("Account: %s\n", account);

  pwent = getpwnam (account);
  if ((! pwent) || (pwent->pw_uid != getuid ()))
    {
      err = gpg_error (GPG_ERR_INTERNAL);	/* FIXME */
      goto out;
    }

  key_path = make_filename ("~", POLDI_PERSONAL_KEY, NULL);
  err = file_to_string (key_path, &key_string);
  if (err)
    goto out;

  err = string_to_sexp (&key_sexp, key_string);
  if (err)
    goto out;

  /* FIXME?  */
  pin = getpass (POLDI_PIN_QUERY_MSG);
  if (! pin)
    {
      err = gpg_error_from_errno (errno);
      goto out;
    }

  err = card_pin_provide (slot, 2, pin);
  if (err)
    goto out;

  err = card_sign (slot, challenge, challenge_n, &signature, &signature_n);
  if (err)
    goto out;

  card_close (slot);
  slot = -1;

  err = challenge_verify (key_sexp,
			  challenge, challenge_n, signature, signature_n);
  if (err)
    printf ("Authentication failed (%s)\n", gpg_strerror (err));
  else
    printf ("Authentication succeeded\n");

 out:

  if (slot != -1)
    card_close (slot);
  free (account);
  free (pin);
  free (serialno);
  free (key_string);
  free (key_path);
  gcry_sexp_release (key_sexp);

  return err;
}

static gpg_error_t
cmd_dump (void)
{
  gcry_sexp_t key;
  char *serial;
  gpg_error_t err;
  int slot;
  char *pin;

  slot = -1;
  serial = NULL;
  key = NULL;

  pin = getpass ("Enter CHV3: ");

  err = card_open (NULL, &slot);
  if (err)
    goto out;

  err = card_init (slot, 0);
  if (err)
    goto out;

  err = card_pin_provide (slot, 3, pin);
  if (err)
    goto out;

  err = card_info (slot, &serial, NULL);
  if (err)
    goto out;

  err = card_read_key (slot, &key);
  if (err)
    goto out;
  
  printf ("Slot: %i\n", slot);
  printf ("Serial: %s\n", serial);
  printf ("Key:\n");
  gcry_sexp_dump (key);

 out:

  if (slot != -1)
    card_close (slot);
  gcry_sexp_release (key);
  free (serial);
  free (pin);

  return err;
}

static gpg_error_t
cmd_list_users (void)
{
  char users_file[] = POLDI_USERS_DB_FILE;
  FILE *users_file_fp;
  gpg_error_t err;
  char *line;
  size_t line_n;
  char *serialno;
  char *account;
  int ret;
  char delimiters[] = "\t\n ";

  line = NULL;

  users_file_fp = fopen (users_file, "r");
  if (! users_file_fp)
    {
      err = gpg_error_from_errno (errno);
      goto out;
    }

  while (1)
    {
      free (line);
      line = NULL;

      ret = getline (&line, &line_n, users_file_fp);
      if (ret == -1)
	{
	  err = gpg_error_from_errno (errno);
	  break;
	}

      serialno = strtok (line, delimiters);
      if (! serialno)
	{
	  err = gpg_error (GPG_ERR_INTERNAL); /* FIXME?  */
	  break;
	}
      account = strtok (NULL, delimiters);
      if (! account)
	{
	  err = gpg_error (GPG_ERR_INTERNAL); /* FIXME?  */
	  break;
	}
      

      printf ("Account: %s; Serial No: %s\n", account, serialno);
    }

 out:

  free (line);
  if (users_file_fp)
    fclose (users_file_fp);	/* FIXME?  */

  return err;
}

static gpg_error_t
cmd_add_user (void)
{
  char users_file[] = POLDI_USERS_DB_FILE;
  const char *serialno;
  const char *account;
  FILE *users_file_fp;
  gpg_error_t err;
  int ret;

  serialno = poldi_ctrl_opt.serialno;
  account = poldi_ctrl_opt.account;

  if (! (serialno && account))
    {
      fprintf (stderr, "Error: Serial number and accounts needs to be given.\n");
      exit (EXIT_FAILURE);
    }

  users_file_fp = fopen (users_file, "a");
  if (! users_file_fp)
    {
      err = gpg_error_from_errno (errno);
      goto out;
    }
  fprintf (users_file_fp, "%s\t%s\n", serialno, account);
  ret = fclose (users_file_fp);
  if (ret)
    {
      err = gpg_error_from_errno (errno);
      goto out;
    }

  err = 0;

 out:

  return err;
}

static gpg_error_t
cmd_remove_user (void)
{
  char users_file_old[] = POLDI_USERS_DB_FILE;
  char users_file_new[] = POLDI_USERS_DB_FILE ".new";
  FILE *users_file_old_fp;
  FILE *users_file_new_fp;
  const char *account;
  char *line;
  size_t line_n;
  size_t account_n;
  gpg_error_t err;
  int ret;
  char *s;

  account = poldi_ctrl_opt.account;
  if (! account)
    {
      fprintf (stderr, "Error: Account needs to be given.\n");
      exit (EXIT_FAILURE);
    }
  account_n = strlen (account);

  line = NULL;
  users_file_old_fp = NULL;
  users_file_new_fp = NULL;

  users_file_old_fp = fopen (users_file_old, "r");
  if (! users_file_old_fp)
    {
      err = gpg_error_from_errno (errno);
      goto out;
    }
  users_file_new_fp = fopen (users_file_new, "w");
  if (! users_file_new_fp)
    {
      err = gpg_error_from_errno (errno);
      goto out;
    }

  while (1)
    {
      free (line);
      line = NULL;
      line_n = 0;

      ret = getline (&line, &line_n, users_file_old_fp);
      if (ret == -1)
	{
	  err = gpg_error_from_errno (errno);
	  break;
	}
      s = line;
      while (*s && (! ((*s == '\t') || (*s == ' '))))
	s++;
      if (! *s)
	{
	  err = gpg_error (GPG_ERR_INTERNAL); /* FIXME? */
	  break;
	}
      while (*s && ((*s == '\t') || (*s == ' ')))
	s++;
      if (! *s)
	{
	  err = gpg_error (GPG_ERR_INTERNAL); /* FIXME? */
	  break;
	}
      
      if ((! strncmp (s, account, account_n)) && s[account_n] == '\n')
	continue;
      fprintf (users_file_new_fp, "%s", line);
      /* FIXME: ferror? */
    }
  if (err)
    goto out;

  fclose (users_file_old_fp);	/* FIXME: it's alright to ignore
				   errors here, right?  */
  users_file_old_fp = NULL;
  ret = fclose (users_file_new_fp);
  users_file_new_fp = NULL;
  if (ret)
    {
      err = gpg_error_from_errno (errno);
      goto out;
    }

  ret = rename (users_file_new, users_file_old);
  if (ret == -1)
    err = gpg_error_from_errno (errno);

 out:

  free (line);
  if (users_file_old_fp)
    fclose (users_file_old_fp);
  if (users_file_new_fp)
    fclose (users_file_new_fp);
  
  return err;
}



static gpg_error_t
cmd_set_key (void)
{
  gpg_error_t err;
  char *path;
  FILE *path_fp;
  int slot;
  char *key_string;
  gcry_sexp_t key_sexp;
  int ret;

  slot = -1;
  path = NULL;
  path_fp = NULL;
  key_sexp = NULL;
  key_string = NULL;

  path = make_filename ("~", POLDI_PERSONAL_KEY, NULL);

  err = card_open (NULL, &slot);
  if (err)
    goto out;

  err = card_init (slot, 0);
  if (err)
    goto out;

  err = card_read_key (slot, &key_sexp);
  if (err)
    goto out;

  card_close (slot);
  slot = -1;

  err = sexp_to_string (key_sexp, &key_string);
  if (err)
    goto out;
  
  path_fp = fopen (path, "w");
  if (! path_fp)
    {
      err = gpg_error_from_errno (errno);
      goto out;
    }

  fprintf (path_fp, "%s", key_string);

  ret = fclose (path_fp);
  path_fp = NULL;
  if (ret)
    {
      err = gpg_error_from_errno (errno);
      goto out;
    }
  
 out:

  free (path);
  if (path_fp)
    fclose (path_fp);
  free (key_string);
  gcry_sexp_release (key_sexp);
  if (slot != -1)
    card_close (slot);

  return err;
}

static gpg_error_t
cmd_show_key (void)
{
  gpg_error_t err;
  char *path;
  char *key_string;

  path = NULL;
  key_string = NULL;

  path = make_filename ("~", POLDI_PERSONAL_KEY, NULL);

  err = file_to_string (path, &key_string);
  if (err)
    goto out;

  printf ("%s", key_string);

 out:

  free (path);
  free (key_string);

  return err;
}


int
main (int argc, char **argv)
{
  int parsing_stage = 0;
  gpg_error_t err;

  set_strusage (my_strusage);
  log_set_prefix ("poldi-ctrl", 1); /* ? */

  err = options_parse_argv (poldi_ctrl_options_cb, &parsing_stage,
			    arg_opts, argc, argv);
  if (err)
    goto out;

  parsing_stage++;
  err = options_parse_conf (poldi_ctrl_options_cb, &parsing_stage,
			    arg_opts, poldi_ctrl_opt.config_file);
  if (err)
    goto out;

  parsing_stage++;
  err = options_parse_argv (poldi_ctrl_options_cb, &parsing_stage,
			    arg_opts, argc, argv);
  if (err)
    goto out;

  scd_init (poldi_ctrl_opt.debug,
	    poldi_ctrl_opt.debug_sc,
	    poldi_ctrl_opt.verbose,
	    poldi_ctrl_opt.ctapi_driver,
	    poldi_ctrl_opt.reader_port,
	    poldi_ctrl_opt.pcsc_driver,
	    poldi_ctrl_opt.disable_opensc,
	    poldi_ctrl_opt.disable_ccid,
	    poldi_ctrl_opt.debug_ccid_driver);

  if ((0
       + (poldi_ctrl_opt.cmd_test)
       + (poldi_ctrl_opt.cmd_set_key)
       + (poldi_ctrl_opt.cmd_show_key)
       + (poldi_ctrl_opt.cmd_add_user)
       + (poldi_ctrl_opt.cmd_remove_user)
       + (poldi_ctrl_opt.cmd_list_users)
       + (poldi_ctrl_opt.cmd_dump)) != 1)
    {
      fprintf (stderr, "Error: no command given (try --help).\n");
      exit (EXIT_FAILURE);
    }

  if (poldi_ctrl_opt.cmd_test)
    err = cmd_test ();
  else if (poldi_ctrl_opt.cmd_dump)
    err = cmd_dump ();
  else if (poldi_ctrl_opt.cmd_set_key)
    err = cmd_set_key ();
  else if (poldi_ctrl_opt.cmd_show_key)
    err = cmd_show_key ();
  else if (poldi_ctrl_opt.cmd_add_user)
    err = cmd_add_user ();
  else if (poldi_ctrl_opt.cmd_remove_user)
    err = cmd_remove_user ();
  else if (poldi_ctrl_opt.cmd_list_users)
    err = cmd_list_users ();

 out:

  if (err)
    {
      fprintf (stderr, "Error: %s\n", gpg_strerror (err));
      exit (1);
    }

  return 0;
}
