/* card.c - High-Level access to OpenPGP smartcards.
   Copyright (C) 2004 g10 Code GmbH.
 
   This file is part of Poldi.
  
   Poldi is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
  
   Poldi is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU Lesser General Public
   License along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include <gcrypt.h>

#include <libscd/apdu.h>
#include <libscd/iso7816.h>
#include <libscd/tlv.h>

#define opt opt_scd

#include "options.h"
#include <../jnlib/xmalloc.h>

#include <syslog.h>

gpg_error_t
card_open (const char *port, int *slot)
{
  gpg_error_t err;
  int slot_new;

  /* Open reader.  */
  slot_new = apdu_open_reader (port);
  if (slot_new == -1)
    {
      err = gpg_error (GPG_ERR_CARD);
      goto out;
    }

  err = 0;
  *slot = slot_new;

 out:

  return err;
}

gpg_error_t
card_init (int slot, int wait_for_card)
{
  /* This is the AID (Application IDentifier) for OpenPGP.  */
  char const aid[] = { 0xD2, 0x76, 0x00, 0x01, 0x24, 0x01 };
  int reader_status;
  gpg_error_t err;
  int ret;
  
  if (wait_for_card)
    {
      while (1)
	{
	  ret = apdu_get_status (slot, 0, &reader_status, NULL);
	  if ((ret != 0) || (reader_status & 3))
	    break;
	  sleep (1);
	}
      if (ret)
	{
	  err = gpg_error (GPG_ERR_CARD);
	  goto out;
	}
    }

  /* Select OpenPGP Application.  */
  err = iso7816_select_application (slot, aid, sizeof (aid));

 out:

  return err;
}

void
card_close (int slot)
{
  apdu_close_reader (slot);
}

gpg_error_t
card_info (int slot, const char **serial_no, const char **fingerprint)
{
  size_t fingerprint_new_n;
  char *fingerprint_new;
  char *serial_no_new;
  const unsigned char *value;
  unsigned char *data;
  size_t value_n;
  size_t data_n;
  gpg_error_t err;
  unsigned int i;

  fingerprint_new = NULL;
  serial_no_new = NULL;
  data = NULL;
  err = 0;

  if (serial_no)
    {
      err = iso7816_get_data (slot, 0x004F, &data, &data_n);
      if (err)
	goto out;

      serial_no_new = malloc ((data_n * 2) + 1);
      if (! serial_no_new)
	{
	  err = gpg_error_from_errno (errno);
	  goto out;
	}

      for (i = 0; i < data_n; i++)
	sprintf (serial_no_new + (i * 2), "%02X", data[i]);
    }

  if (fingerprint)
    {
      free (data);
      data = NULL;

      err = iso7816_get_data (slot, 0x6E, &data, &data_n);
      if (err)
	goto out;

      value = find_tlv (data, data_n, 0x00C5, &value_n);
      if (! (value
	     && (! (value_n > (data_n - (value - data))))
	     && (value_n >= 60))) /* FIXME: Shouldn't this be "==
				     60"?  */
	{
	  err = gpg_error (GPG_ERR_INTERNAL);
	  goto out;
	}

      fingerprint_new_n = 41;
      fingerprint_new = malloc (fingerprint_new_n);
      if (! fingerprint_new)
	{
	  err = gpg_error_from_errno (errno);
	  goto out;
	}
      
      /* Copy out third key FPR.  */
      for (i = 0; i < 20; i++)
	sprintf (fingerprint_new + (i * 2), "%02X", (value + (2 * 20))[i]);
    }

 out:

  free (data);
  
  if (! err)
    {
      if (serial_no)
	*serial_no = (const char *) serial_no_new;
      if (fingerprint)
	*fingerprint = (const char *) fingerprint_new;
    }
  else
    {
      free (serial_no_new);
      free (fingerprint_new);
    }

  return err;
}

gpg_error_t
card_read_key (int slot, gcry_sexp_t *key)
{
  const unsigned char *data;
  const unsigned char *e;
  const unsigned char *n;
  unsigned char *buffer;
  size_t buffer_n;
  size_t data_n;
  size_t e_n;
  size_t n_n;
  gcry_mpi_t e_mpi;
  gcry_mpi_t n_mpi;
  int rc;
  gpg_error_t err;
  gcry_sexp_t key_sexp;

  buffer = NULL;
  data = NULL;
  e = NULL;
  n = NULL;
  e_mpi = NULL;
  n_mpi = NULL;
  key_sexp = NULL;

  rc = iso7816_read_public_key (slot, "\xA4", 2, &buffer, &buffer_n);
  if (rc)
    {
      err = gpg_error (GPG_ERR_CARD);
      goto out;
    }

  /* Extract key data.  */
  data = find_tlv (buffer, buffer_n, 0x7F49, &data_n);
  if (! data)
    {
      err = gpg_error (GPG_ERR_CARD);
      goto out;
    }

  /* Extract n.  */
  n = find_tlv (data, data_n, 0x0081, &n_n);
  if (! n)
    {
      err = gpg_error (GPG_ERR_CARD);
      goto out;
    }
  
  /* Extract e.  */
  e = find_tlv (data, data_n, 0x0082, &e_n);
  if (! e)
    {
      err = gpg_error (GPG_ERR_CARD);
      goto out;
    }

  err = gcry_mpi_scan (&n_mpi, GCRYMPI_FMT_USG, n, n_n, NULL);
  if (err)
    goto out;

  err = gcry_mpi_scan (&e_mpi, GCRYMPI_FMT_USG, e, e_n, NULL);
  if (err)
    goto out;

  err = gcry_sexp_build (&key_sexp, NULL,
			 "(public-key (rsa (n %m) (e %m)))", n_mpi, e_mpi);
  if (err)
    goto out;

  *key = key_sexp;

 out:

  free (buffer);
  gcry_mpi_release (e_mpi);
  gcry_mpi_release (n_mpi);

  return err;
}

gpg_error_t
card_pin_provide (int slot, int which, const unsigned char *pin)
{
  gpg_error_t err = GPG_ERR_NO_ERROR;
  int chv_id;

  if (which == 1)
    chv_id = 0x81;
  else if (which == 2)
    chv_id = 0x82;
  else if (which == 3)
    chv_id = 0x83;
  else
    {
      err = gpg_error (GPG_ERR_INV_ARG);
      goto out;
    }

  err = iso7816_verify (slot, chv_id, pin, strlen (pin));

 out:

  return err;
}


gpg_error_t
card_sign (int slot, const unsigned char *data, size_t data_n,
	   unsigned char **data_signed, size_t *data_signed_n)
{
  gpg_error_t err = GPG_ERR_NO_ERROR;
  unsigned char *digestinfo = NULL;
  size_t digestinfo_n = 0;
  unsigned char md_asn[100] = {};
  size_t md_asn_n = sizeof (md_asn);

  err = gcry_md_get_asnoid (GCRY_MD_SHA1, md_asn, &md_asn_n);
  if (! err)
    {
      digestinfo_n = md_asn_n + data_n;
      digestinfo = malloc (digestinfo_n);
      if (! digestinfo)
	err = GPG_ERR_ENOMEM;
    }

  if (! err)
    {
      memcpy (digestinfo, md_asn, md_asn_n);
      memcpy (digestinfo + md_asn_n, data, data_n);
      
      err = iso7816_internal_authenticate (slot, digestinfo, digestinfo_n,
					   data_signed, data_signed_n);
    }

  if (digestinfo)
    free (digestinfo);

  return err;
}
