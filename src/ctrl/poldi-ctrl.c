/* poldi-ctrl.c - Poldi maintaince tool
   Copyright (C) 2004, 2005, 2007 g10 Code GmbH.
 
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
#include <fcntl.h>
#include <pwd.h>
#include <assert.h>

#include <gcrypt.h>

#include <jnlib/argparse.h>
#include <jnlib/xmalloc.h>
#include <jnlib/logging.h>
#include <common/optparse.h>
#include <common/support.h>
#include <common/usersdb.h>
#include <common/defs.h>
#include "common/support.h"

#include <scd/scd.h>



#define KEY_FILE_CREATE_MODE 0644



/* Global flags.  */
struct poldi_ctrl_opt
{
  unsigned int debug; /* debug flags (DBG_foo_VALUE) */
  int verbose;      /* verbosity level */

  char *config_file;
  char *account;
  char *serialno;
  int serialno_inserted;
  int require_card_switch;
  int cmd_dump;
  int cmd_register_card;
  int cmd_unregister_card;
  int cmd_set_key;
  int cmd_show_key;
  int cmd_list_users;
  int cmd_list_cards;
  int cmd_associate;
  int cmd_disassociate;
  unsigned int wait_timeout;
} poldi_ctrl_opt;

/* Set defaults.  */
struct poldi_ctrl_opt poldi_ctrl_opt =
  {
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
    0,
    0,
    0,
    0,
    0
  };

enum arg_opt_ids
  {
    arg_dump = 'd',

    arg_register_card = 'r',
    arg_unregister_card = 'u',
    arg_list_cards = 'l',

    arg_set_key = 's',
    arg_show_key = 'k',

    arg_associate = 'a',
    arg_disassociate = 'D',
    arg_list_users  = 'L',

    arg_config_file = 'c',
    arg_serialno_inserted = 'i',

    arg_account = 500,
    arg_serialno,
    arg_debug,
    //    arg_require_card_switch,
    arg_wait_timeout
  };

static ARGPARSE_OPTS arg_opts[] =
  {
    /* Commands:  */

    { 300, NULL, 0, "@Commands:\n " },
    { arg_dump,
      "dump",        256, "Dump certain card information"      },
    { arg_register_card,
      "register-card",    256, "Register new smartcard"        },
    { arg_unregister_card,
      "unregister-card", 256,  "Unregister smartcard"          },
    { arg_list_cards,
      "list-cards", 256,  "List registered smartcards"         },
    { arg_list_users,
      "list-users",    256, "List accounts from users db"      },
    { arg_associate,
      "associate",     256, "Associate user with smartcard"    },
    { arg_disassociate,
      "disassociate",     256, "Disassociate a user from smartcard"      },
    { arg_set_key,
      "set-key",     256, "Set key for calling user"           },
    { arg_show_key,
      "show-key",     256, "Show key of calling user"          },

    /* Options:  */
    { 301, NULL, 0, "@\nOptions:\n " },
    { arg_config_file,
      "config-file",   2, "|FILE|Specify configuration file"   },
    { arg_debug,
      "debug", 256, "Enable debugging mode" },
    { arg_account,
      "account",       2, "|NAME|Specify Unix account"         },
    { arg_serialno,
      "serialno",      2, "|NAME|Specify card serial number"   },
    { arg_serialno_inserted,
      "serialno-inserted", 256, "Use serial number from currently inserted card"   },
    //    { arg_require_card_switch,
    //      "require-card-switch", 0, "Require re-insertion of card" },
    { arg_wait_timeout,
      "wait-timeout", 1, "|SEC|Specify timeout for waiting" },
    { 0,
      NULL,            0, NULL                                 }
  };



/* Callback function printing program usage information used through
   jnlib.  */
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

    case 14:
      p = "Copyright (C) 2005, 2007 g10 Code GmbH";
      break;

    case 19:
      p = "Please report bugs to <" PACKAGE_BUGREPORT ">\n";
      break;

    case 1:
    case 40:
      p = "Usage: poldi-ctrl <command> [options]";
      break;

    case 41:
      p = "Syntax: poldi-ctrl <command> [options]\n";
      break;

    default:
      p = NULL;
    }

  return p;
}

/* Option parser callback by options_parse_argv() and
   options_parse_conf(), which are jnlib wrappers.  */
static gpg_error_t
poldi_ctrl_options_cb (ARGPARSE_ARGS *parg, void *opaque)
{
  int parsing_stage = *((int *) opaque);
  gpg_error_t err = 0;

  switch (parg->r_opt)
    {
    case arg_config_file:
      if (! parsing_stage)
	poldi_ctrl_opt.config_file = xstrdup (parg->r.ret_str);
      break;

    case arg_set_key:
      if (parsing_stage)
	poldi_ctrl_opt.cmd_set_key = 1;
      break;

    case arg_associate:
      if (parsing_stage)
	poldi_ctrl_opt.cmd_associate = 1;
      break;

    case arg_disassociate:
      if (parsing_stage)
	poldi_ctrl_opt.cmd_disassociate = 1;
      break;

    case arg_show_key:
      if (parsing_stage)
	poldi_ctrl_opt.cmd_show_key = 1;
      break;

    case arg_dump:
      if (parsing_stage)
	poldi_ctrl_opt.cmd_dump = 1;
      break;

    case arg_register_card:
      if (parsing_stage)
	poldi_ctrl_opt.cmd_register_card = 1;
      break;

    case arg_unregister_card:
      if (parsing_stage)
	poldi_ctrl_opt.cmd_unregister_card = 1;
      break;

    case arg_list_cards:
      if (parsing_stage)
	poldi_ctrl_opt.cmd_list_cards = 1;
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
      
    case arg_serialno_inserted:
      if (parsing_stage)
	poldi_ctrl_opt.serialno_inserted = 1;
      break;
      
    case arg_debug:
      if (parsing_stage)
	{
	  poldi_ctrl_opt.debug = ~0;
	  poldi_ctrl_opt.verbose = 1;
	}
      break;

#if 0
    case arg_require_card_switch:
      if (parsing_stage)
	poldi_ctrl_opt.require_card_switch = 1;
      break;
#endif

    case arg_wait_timeout:
      if (parsing_stage)
	poldi_ctrl_opt.wait_timeout = parg->r.ret_int;
      break;

    default:
      parg->err = 2;
      break;
    }

  return gpg_error (err);
}



/*
 * User interaction.
 */

static gcry_error_t
ask_user (const char *prompt, char **answer)
{
  gcry_error_t err;
  size_t buffer_n;
  char *buffer;
  ssize_t ret;
  char *c;

  fprintf (stderr, "%s: ", prompt);
  fflush (stderr);

  /* Read single line of data.  */

  buffer = NULL;
  buffer_n = 0;
  ret = getline (&buffer, &buffer_n, stdin);
  if (ret == -1)
    {
      if (ferror (stdin))
	err = gcry_error_from_errno (errno);
      else
	err = gcry_error (GPG_ERR_NO_DATA);
      goto out;
    }
  else
    err = 0;

  /* We got a line of data.  */

  /* Remove newline.  */

  c = strchr (buffer, '\n');
  if (c)
    *c = '\0';

  if (! strlen (buffer))
    {
      /* If this yields an empty line, bail out.  */

      free (buffer);
      buffer_n = 0;
      buffer = NULL;
      err = gcry_error (GPG_ERR_INV_NAME);
    }
  else
    *answer = buffer;

 out:

  return err;
}



/*
 * Key file management.
 */

/* Create a key file for user ACCOUNT and card serial number SERIALNO.
   Return proper error code.  */
static gpg_error_t
key_file_create (struct passwd *pwent, const char *serialno)
{
  struct stat statbuf;
  gpg_error_t err;
  char *path;
  int ret;
  int fd;

  path = NULL;

  err = key_filename_construct (&path, serialno);
  if (err)
    {
      log_error ("Error: failed to construct key file path "
		 "for serial number `%s': %s\n",
		 serialno, gpg_strerror (err));
      goto out;
    }

  fd = open (path, O_WRONLY | O_CREAT | O_EXCL, KEY_FILE_CREATE_MODE);
  if (fd == -1)
    {
      err = gcry_error_from_errno (errno);
      if (gcry_err_code (err) == GPG_ERR_EEXIST)
	{
	  log_error ("Warning: key file `%s' does already exist, skipping\n",
		     path);
	  err = 0;
	}
      else
	log_error ("Error: failed to open key file `%s' for writing: %s\n",
		   path, gcry_strerror (err));
      goto out;
    }

  ret = close (fd);
  if (ret == -1)
    {
      err = gpg_error_from_errno (errno);
      log_error ("Error: failed to close key file `%s': %s\n",
		 path, gpg_strerror (err));
      goto out;
    }

  if (pwent)
    {
      /* Adjust access control.  */

      ret = stat (path, &statbuf);
      if (ret == -1)
	{
	  err = gpg_error_from_errno (errno);
	  log_error ("Error: failed to stat key file `%s': %s\n",
		     path, gpg_strerror (err));
	  goto out;
	}

      ret = chown (path, pwent->pw_uid, statbuf.st_gid);
      if (ret == -1)
	{
	  err = gpg_error_from_errno (errno);
	  log_error ("Warning: failed to chown key file `%s' to (%i, %i): %s\n",
		     path, pwent->pw_uid, statbuf.st_gid, gpg_strerror (err));
	  goto out;
	}
    }

  err = 0;

 out:

  free (path);

  return err;
}

/* Remove the key file for card serial number SERIALNO.  Return proper
   error code.  */
static gpg_error_t
key_file_remove (const char *serialno)
{
  gpg_error_t err;
  char *path;
  int ret;

  err = key_filename_construct (&path, serialno);
  if (err)
    {
      log_error ("Error: failed to construct key file path "
		 "for serial number `%s': %s\n",
		 serialno, gpg_strerror (err));
      goto out;
    }

  ret = unlink (path);
  if ((ret == -1) && (errno != ENOENT))
    {
      err = gpg_error_from_errno (errno);
      log_error ("Error: failed to unlink key file `%s': %s\n",
		 path, gpg_strerror (err));
      goto out;
    }

  err = 0;

 out:

  free (path);

  return err;
}



/*
 * Command functions.
 */

static gpg_error_t
conversation (conversation_type_t type, void *opaque,
	      const char *info, char **response)
{
  gpg_error_t err;

  assert ((type == CONVERSATION_TELL)
	  || (type == CONVERSATION_ASK_SECRET));

  err = 0;
  switch (type)
    {
    case CONVERSATION_TELL:
      printf ("%s\n", info);
      err = 0;
      break;

    case CONVERSATION_ASK_SECRET:
      {
	char *secret;

	secret = getpass (info);
	if (! secret)
	  {
	    err = gpg_error_from_errno (errno);
	    log_error ("Error: getpass() returned NULL: %s\n",
		       gpg_strerror (err));
	  }
	else
	  {
	    *response = secret;
	    err = 0;
	  }
      }
      break;
    }

  return err;
}

static struct scd_cardinfo cardinfo_NULL;



/* Implementation of `dump' command; dumps information from card.  */
static gpg_error_t
cmd_dump (void)
{
  gcry_sexp_t key;
  char *key_s;
  gpg_error_t err;
  //unsigned int version;
  struct scd_cardinfo cardinfo;
  char fpr[41];
  scd_context_t ctx;

  ctx = NULL;
  cardinfo = cardinfo_NULL;
  key = NULL;
  key_s = NULL;
  //pin = NULL;

  /* Connect.  */

  err = scd_connect (&ctx,
		     getenv ("GPG_AGENT_INFO"),
		     NULL,
		     0);
  if (err)
    {
      log_error ("Error: scd_connect() failed: %s\n",
		 gpg_strerror (err));
      goto out;
    }

  /* Retrieve more card information.  */

  err = scd_learn (ctx, &cardinfo);
  if (err)
    {
      log_error ("Error: scd_learn() failed: %s\n",
		 gpg_strerror (err));
      goto out;
    }

#if 0

  /* FIXME, moritz, dunno wether we still need this with
     gpg-agent.  */

  if (version <= 0x0100)
    {
      /* These cards contain a bug, which makes it necessary to pass
	 CHV3 to the card before reading out the public key.  */

      printf (POLDI_OLD_CARD_KEY_RETRIVAL_EXPLANATION, version);

      pin = getpass (POLDI_PIN3_QUERY_MSG);
      if (! pin)
	{
	  err = gpg_error_from_errno (errno);
	  log_error ("Error: failed to retrieve PIN from user: %s\n",
		     gpg_strerror (err));
	  goto out;
	}
	
      err = card_pin_provide (slot, 3, pin);
      if (err)
	{
	  log_error ("Error: failed to send PIN to card: %s\n",
		     gpg_strerror (err));
	  goto out;
	}
    }

#endif

  /* Retrieve key from card.  */

  err = scd_readkey (ctx, "OPENPGP.3", &key);
  //  err = agent_scd_readkey ("OPENPGP.3", &key);
  if (err)
    {
      log_error ("Error: failed to retrieve key from card: %s\n",
		 gpg_strerror (err));
      goto out;
    }

  /* Convert key into a string.  */

  err = sexp_to_string (key, &key_s);
  if (err)
    {
      log_error ("Error: failed to convert key S-Expression "
		 "into C-String: %s\n",
		 gpg_strerror (err));
      goto out;
    }

  convert_to_hex (cardinfo.fpr3, 20, fpr);

  printf ("Serial number: %s\n"
	  "Signing key fingerprint: %s\n"
	  "Key:\n%s\n",
	  cardinfo.serialno, fpr, key_s);

 out:

  scd_reset (ctx);

  gcry_sexp_release (key);
  gcry_free (key_s);
  //free (pin);

  scd_release_cardinfo (&cardinfo);
  scd_disconnect (ctx);

  return err;
}



/* Implementation of `list-users' command; dump information from user
   database.  */
static gpg_error_t
cmd_list_users (void)
{
  gcry_error_t err;

  err = usersdb_list (stdout);
  /* FIXME.  */

  return err;
}



/* Return the SERIALNO of the currently inserted smartcard.  */
static gpg_error_t
serialno_from_inserted_card (char **serialno)
{
  scd_context_t ctx;
  gpg_error_t err;

  *serialno = NULL;
  ctx = NULL;

  err = scd_connect (&ctx,
		     getenv ("GPG_AGENT_INFO"),
		     NULL,
		     0);
  if (err)
    {
      log_error ("Error: scd_connect() failed: %s\n",
		 gpg_strerror (err));
      goto out;
    }

  err = scd_serialno (ctx, serialno);
  if (err)
    {
      log_error ("Error: scd_serialno() failed: %s\n",
		 gpg_strerror (err));
      goto out;
    }

 out:

  scd_disconnect (ctx);

  return err;
}

/* Implementation of `register-card' command.  */
/* FIXME: shouldn't register-card simply register the card INSERTED
   instead of requiring a specified serialno on the CLI? -mo  */
static gpg_error_t
cmd_register_card (void)
{
  struct passwd *pwent;
  gpg_error_t err;
  char *serialno;
  char *account;
  int use_inserted;

  serialno = poldi_ctrl_opt.serialno;
  account = poldi_ctrl_opt.account;
  use_inserted = poldi_ctrl_opt.serialno_inserted;

  if (! serialno)
    {
      if (use_inserted)
	{
	  /* Try   to  retrieve   serialno  from   currently  inserted
	     card.  */
	  err = serialno_from_inserted_card (&serialno);
	  if (err)
	    {
	      log_error ("Error: failed to retrieve serial "
			 "number from inserted card: %s\n",
			 gpg_strerror (err));
	      goto out;
	    }
	}
      else
	{
	  log_error ("Error: serial number needs to be given\n");
	  err = gcry_error (GPG_ERR_INV_PARAMETER);
	  goto out;
	}
    }

  if (! account)
    {
      log_error ("Warning: no account specified, therefore the key "
		 "file will be owned by the current user\n");
      pwent = NULL;
    }
  else
    {
      /* Lookup user in password database.  */

      pwent = getpwnam (account);
      if (! pwent)
	{
	  log_error ("Error: unknown user `%s'\n", account);
	  err = gcry_error (GPG_ERR_INV_NAME);
	  goto out;
	}
    }

  /* Create empty key file.  */

  err = key_file_create (pwent, serialno);
  if (err)
    {
      log_error ("Error: failed to create key file for "
		 "serial number: %s\n",
		 serialno);
      goto out;
    }

 out:

  if (serialno != poldi_ctrl_opt.serialno)
    free (serialno);		/* FIXME, moritz: which free? */

  return err;
}

/* Implementation of `unregister-card' command.  */
static gpg_error_t
cmd_unregister_card (void)
{
  gpg_error_t err;
  char *serialno;
  int use_inserted;

  serialno = poldi_ctrl_opt.serialno;
  use_inserted = poldi_ctrl_opt.serialno_inserted;

  if (! serialno)
    {
      if (use_inserted)
	{
	  /* Try   to  retrieve   serialno  from   currently  inserted
	     card.  */
	  err = serialno_from_inserted_card (&serialno);
	  if (err)
	    {
	      log_error ("Error: failed to retrieve serial "
			 "number from inserted card: %s\n",
			 gpg_strerror (err));
	      goto out;
	    }
	}
      else
	{
	  log_error ("Error: serial number needs to be given\n");
	  err = gcry_error (GPG_ERR_INV_PARAMETER);
	  goto out;
	}
    }

  /* FIXME: print warning in case accounts are still connected with
     that card?  */

  err = key_file_remove (poldi_ctrl_opt.serialno);
  if (err)
    {
      log_error ("Error: failed to remove key file for "
		 "serial number `%s': %s\n",
		 poldi_ctrl_opt.serialno, gpg_strerror (err));
      goto out;
    }

 out:

  if (serialno != poldi_ctrl_opt.serialno)
    free (serialno);		/* FIXME, moritz: which free? */

  return err;
}



static gpg_error_t
directory_process_cb (void *opaque, struct dirent *dirent)
{
  int length;

  length = strlen (dirent->d_name);

  if (length == 32)		/* FIXME? */
    printf ("%s\n", dirent->d_name);

  return 0;
}

/* Implementation of `list-cards' command.  */
static gpg_error_t
cmd_list_cards (void)
{
  gpg_error_t err;

  err = directory_process (POLDI_KEY_DIRECTORY, directory_process_cb, NULL);

  return err;
}



static gpg_error_t
cmd_associate (void)
{
  struct passwd *pwent;
  gpg_error_t err;
  char *serialno;
  char *account;
  int use_inserted;

  serialno = poldi_ctrl_opt.serialno;
  account = poldi_ctrl_opt.account;
  use_inserted = poldi_ctrl_opt.serialno_inserted;

  if (!account)
    {
      log_error ("Error: accounts needs to be given\n");
      err = gcry_error (GPG_ERR_INV_PARAMETER);
      goto out;
    }

  if (!serialno)
    {
      if (use_inserted)
	{
	  /* Try   to  retrieve   serialno  from   currently  inserted
	     card.  */
	  err = serialno_from_inserted_card (&serialno);
	  if (err)
	    {
	      log_error ("Error: failed to retrieve serial "
			 "number from inserted card: %s\n",
			 gpg_strerror (err));
	      goto out;
	    }
	}
      else
	{
	  log_error ("Error: serial number needs to be given\n");
	  err = gcry_error (GPG_ERR_INV_PARAMETER);
	  goto out;
	}
    }

  /* Lookup user in password database.  */

  pwent = getpwnam (account);
  if (! pwent)
    {
      log_error ("Error: unknown user `%s'\n", account);
      err = gcry_error (GPG_ERR_INV_NAME);
      goto out;
    }

  err = usersdb_add (account, serialno);
  if (err)
    {
      log_error ("Error: failed to add entry to user database: %s\n",
		 gpg_strerror (err));
      goto out;
    }

  printf ("Associated account `%s' with card `%s'\n",
	  account, serialno);

 out:

  if (serialno != poldi_ctrl_opt.serialno)
    free (serialno);		/* FIXME, moritz: which free? */

  return err;
}

static gpg_error_t
cmd_disassociate (void)
{
  gpg_error_t err;
  char *serialno;
  char *account;
  int use_inserted;

  serialno = poldi_ctrl_opt.serialno;
  account = poldi_ctrl_opt.account;
  use_inserted = poldi_ctrl_opt.serialno_inserted;

  if (!account)
    {
      log_error ("Error: accounts needs to be given\n");
      err = gcry_error (GPG_ERR_INV_PARAMETER);
      goto out;
    }

  if (!serialno)
    {
      if (use_inserted)
	{
	  /* Try   to  retrieve   serialno  from   currently  inserted
	     card.  */
	  err = serialno_from_inserted_card (&serialno);
	  if (err)
	    {
	      log_error ("Error: failed to retrieve serial "
			 "number from inserted card: %s\n",
			 gpg_strerror (err));
	      goto out;
	    }
	}
      else
	{
	  log_error ("Error: serial number needs to be given\n");
	  err = gcry_error (GPG_ERR_INV_PARAMETER);
	  goto out;
	}
    }

  /* COMMENT.  */
  
  err = usersdb_remove (account, serialno);
  if (err)
    {
      log_error ("Error: failed to remove entry for user `%s' "
		 "and serial number `%s' from user database: %s\n",
		 account, serialno, gpg_strerror (err));
      goto out;
    }

 out:

  if (serialno != poldi_ctrl_opt.serialno)
    free (serialno);		/* FIXME, moritz: which free? */

  return err;
}




/* Implementation of `set-key' command; install key of currently
   inserted card for user requesting this action.  */

static gpg_error_t
cmd_set_key (void)
{
  gpg_error_t err;
  char *path;
  FILE *path_fp;
  char *key_string;
  char *serialno;
  char *pin;
  gcry_sexp_t key_sexp;
  unsigned int version;
  int ret;
  struct scd_cardinfo cardinfo;
  scd_context_t ctx;

  ctx = NULL;
  cardinfo = cardinfo_NULL;
  pin = NULL;
  path = NULL;
  path_fp = NULL;
  serialno = NULL;
  key_sexp = NULL;
  key_string = NULL;
  version = 0;

  /* Connect.  */

  err = scd_connect (&ctx,
		     getenv ("GPG_AGENT_INFO"),
		     NULL,
		     0);
  if (err)
    {
      log_error ("Error: scd_connect() failed: %s\n",
		 gpg_strerror (err));
      goto out;
    }

  /* Retrieve basic information from card.  */

  err = scd_learn (ctx, &cardinfo);
  if (err)
    {
      log_error ("Error: failed to retreive basic information"
		 "from card: %s\n",
		 gpg_strerror (err));
      goto out;
    }

  /* Construct key path.  */

  err = key_filename_construct (&path, cardinfo.serialno);
  if (err)
    {
      log_error ("Error: failed to construct key filename: %s\n",
		 gpg_strerror (err));
      goto out;
    }

#if 0 /* FIXME, moritz? */

  if (version <= 0x0100)
    {
      /* Special handling necessary.
	 .
	 These cards contain a bug, which makes it necessary to pass
	 CHV3 to the card before reading out the public key.  */

      log_info (POLDI_OLD_CARD_KEY_RETRIVAL_EXPLANATION, version);

      /* Retrieve PIN from user.  */

      pin = getpass (POLDI_PIN3_QUERY_MSG);
      if (! pin)
	{
	  err = gpg_error_from_errno (errno);
	  log_error ("Error: failed to retrieve PIN from user: %s\n",
		     gpg_strerror (err));
	  goto out;
	}

      /* Send PIN to card.  */

      err = card_pin_provide (slot, 3, pin);
      if (err)
	{
	  log_error ("Error: failed to send PIN to card: %s\n",
		     gpg_strerror (err));
	  goto out;
	}
    }

#endif

  /* Retrieve key from card.  */

  err = scd_readkey (ctx, "OPENPGP.3", &key_sexp);
  if (err)
    {
      log_error ("Error: failed to retrieve key from card: %s\n",
		 gpg_strerror (err));
      goto out;
    }

  /* Convert key into a string.  */

  err = sexp_to_string (key_sexp, &key_string);
  if (err)
    {
      log_error ("Error: failed to convert key S-Expression "
		 "into C-String: %s\n",
		 gpg_strerror (err));
      goto out;
    }

  /* Write key to key file.  */

  path_fp = fopen (path, "w");
  if (! path_fp)
    {
      err = gpg_error_from_errno (errno);
      log_error ("Error: failed to open key file `%s': %s\n",
		 path, gpg_strerror (err));
      goto out;
    }

  fprintf (path_fp, "%s", key_string);

  ret = fclose (path_fp);
  path_fp = NULL;
  if (ret)
    {
      err = gpg_error_from_errno (errno);
      log_error ("Error: failed to successfully close key file `%s' "
		 "buffered data might have not been flushed correctly: %s\n",
		 path, gpg_strerror (err));
      goto out;
    }

 out:

  scd_reset (ctx);

  free (pin);
  free (path);
  free (serialno);
  if (path_fp)
    fclose (path_fp);
  free (key_string);
  gcry_sexp_release (key_sexp);
  scd_release_cardinfo (&cardinfo);
  scd_disconnect (ctx);

  return err;
}



/* Implementation of `show-key' command; sends to content of key file
   to standard output.  */
static gpg_error_t
cmd_show_key (void)
{
  gpg_error_t err;
  char *path;
  char *key_string;
  char *serialno;
  const char *username;
  gcry_sexp_t key_sexp;

  path = NULL;
  serialno = NULL;
  key_sexp = NULL;
  key_string = NULL;

  if (poldi_ctrl_opt.serialno)
    err = key_filename_construct (&path, poldi_ctrl_opt.serialno);
  else
    {
      /* Retrieve username of caller.  */
      err = lookup_own_username (&username);
      if (err)
	{
	  log_error ("Error: failed to lookup own username: %s\n",
		     gpg_strerror (err));
	  goto out;
	}
      
      /* Lookup serial number for username.  */

      err = usersdb_lookup_by_username (username, &serialno);
      if (gcry_err_code (err) == GPG_ERR_AMBIGUOUS_NAME)
	err = ask_user ("Need to figure out the serialno", &serialno);

      if (err)
	{
	  log_error ("Error: failed to lookup serial number "
		     "for user `%s': %s\n",
		     username, gpg_strerror (err));
	  goto out;
	}

      /* Construct key path.  */

      err = key_filename_construct (&path, serialno);
    }
  
  if (err)
    {
      log_error ("Error: failed to construct key file path "
		 "for serial number `%s': %s\n",
		 (poldi_ctrl_opt.serialno
		  ? poldi_ctrl_opt.serialno : serialno),
		 gcry_strerror (err));
      goto out;
    }

  /* Read key file content into string.  */

  err = file_to_string (path, &key_string);
  if (err)
    {
      log_error ("Error: key could not be read from key file `%s': %s\n",
		 path, gpg_strerror (err));
      goto out;
    }

  /* Convert into S-Expression.  */

  err = string_to_sexp (&key_sexp, key_string);
  if (err)
    {
      log_error ("Error: failed to convert key into S-Expression: %s\n",
		 gpg_strerror (err));
      goto out;
    }

  /* And back into C-string.  */

  free (key_string);
  key_string = NULL;
  err = sexp_to_string (key_sexp, &key_string);
  if (err)
    {
      log_error ("Error: failed to convert key S-Expression "
		 "into C-String: %s\n",
		 gpg_strerror (err));
      goto out;
    }

  /* And print key string.  */

  if (key_string)
    printf ("%s", key_string);

 out:

  free (path);
  free (key_string);
  free (serialno);
  gcry_sexp_release (key_sexp);

  return err;
}



/* Main.  */
int
main (int argc, char **argv)
{
  unsigned int parsing_stage;
  unsigned int ncommands;
  gpg_error_t err;

  /* Initialize jnlib subsystems.  */

  /* Initialize Libgcrypt.  */
  gcry_control (GCRYCTL_INIT_SECMEM, 16384, 0);

  set_strusage (my_strusage);
  log_set_prefix ("poldi-ctrl", 1);

  /* Parse argument vector, looking for a file config-file
     paramater.  */

  parsing_stage = 0;

  err = options_parse_argv (poldi_ctrl_options_cb, &parsing_stage,
			    arg_opts, argc, argv);
  if (err)
    {
      log_error ("Error: parsing argument vector (stage: %u) failed: %s\n",
		 parsing_stage, gpg_strerror (err));
      goto out;
    }

  /* Parse config file.  */

  parsing_stage++;
  err = options_parse_conf (poldi_ctrl_options_cb, &parsing_stage,
			    arg_opts, poldi_ctrl_opt.config_file);
  if (err)
    {
      log_error ("Error: parsing config file failed: %s\n",
		 gpg_strerror (err));
      goto out;
    }

  /* And finally the argument vector, overwriting values from config
     file.  */
  parsing_stage++;
  err = options_parse_argv (poldi_ctrl_options_cb, &parsing_stage,
			    arg_opts, argc, argv);
  if (err)
    {
      log_error ("Error: parsing argument vector (stage: %u) failed: %s\n",
		 parsing_stage, gpg_strerror (err));
      goto out;
    }

  ncommands = (0
	       + poldi_ctrl_opt.cmd_set_key
	       + poldi_ctrl_opt.cmd_associate
	       + poldi_ctrl_opt.cmd_disassociate
	       + poldi_ctrl_opt.cmd_show_key
	       + poldi_ctrl_opt.cmd_register_card
	       + poldi_ctrl_opt.cmd_unregister_card
	       + poldi_ctrl_opt.cmd_list_cards
	       + poldi_ctrl_opt.cmd_list_users
	       + poldi_ctrl_opt.cmd_dump);
  if (ncommands > 1)
    {
      log_error ("Error: more than one command specified (try --help)\n");
      exit (EXIT_FAILURE);
    }
  else if (! ncommands)
    {
      log_error ("Error: no command specified (try --help)\n");
      exit (EXIT_FAILURE);
    }

  if (poldi_ctrl_opt.cmd_dump)
    err = cmd_dump ();
  else if (poldi_ctrl_opt.cmd_set_key)
    err = cmd_set_key ();
  else if (poldi_ctrl_opt.cmd_associate)
    err = cmd_associate ();
  else if (poldi_ctrl_opt.cmd_disassociate)
    err = cmd_disassociate ();
  else if (poldi_ctrl_opt.cmd_show_key)
    err = cmd_show_key ();
  else if (poldi_ctrl_opt.cmd_register_card)
    err = cmd_register_card ();
  else if (poldi_ctrl_opt.cmd_unregister_card)
    err = cmd_unregister_card ();
  else if (poldi_ctrl_opt.cmd_list_cards)
    err = cmd_list_cards ();
  else if (poldi_ctrl_opt.cmd_list_users)
    err = cmd_list_users ();

 out:
  
  return err ? EXIT_FAILURE : EXIT_SUCCESS;
}

/* END */
