/* auth-methods.h - Auth method definitions (Poldi)
   Copyright (C) 2008 g10 Code GmbH
 
   This file is part of Poldi.
 
   Poldi is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
 
   Poldi is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
 
   You should have received a copy of the GNU General Public License
   along with this program; if not, see
   <http://www.gnu.org/licenses/>.  */

#ifndef POLDI_AUTH_METHODS_H
#define POLDI_AUTH_METHODS_H

#include <poldi.h>

#include <util/simpleparse.h>

#include "auth-support/ctx.h"



/* This function is called at initialization time (before
   authentication method specific argument parsing is done) in order
   to properly setup an authentication method specific cookie
   object.  */
typedef gpg_error_t (*auth_method_func_init_t) (void **cookie);

/* This function is called after an authentication run in order to
   free any resources allocated through the call to the initialization
   function.  */
typedef void (*auth_method_func_deinit_t) (void *cookie);

// We don't need this, we can simply use the cb type from simpleparse!
///* This function is a callback implementing the knowledge for
//   authentication method specific argument parsing.  */
//typedef gpg_error_t (*auth_method_func_parsecb_t) (ARGPARSE_ARGS *parg, void *cookie);

/* This function is called in order to authenticate a user.  The
   identity of the user after authentication is stored in
   *USERNAME. */
typedef int (*auth_method_func_auth_t) (poldi_ctx_t ctx, void *cookie,
					char **username);

/* This function is called in order to authenticate a user as
   USERNAME.  */
typedef int (*auth_method_func_auth_as_t) (poldi_ctx_t ctx, void *cookie,
					   const char *username);

struct auth_method_parse_cookie
{
  poldi_ctx_t poldi_ctx;
  void *method_ctx;
};

/* Each authentication method must define one such object.  */
struct auth_method_s
{
  auth_method_func_init_t func_init;
  auth_method_func_deinit_t func_deinit;
  auth_method_func_auth_t func_auth;
  auth_method_func_auth_as_t func_auth_as;
  simpleparse_opt_spec_t *opt_specs;
  simpleparse_parse_cb_t parsecb;
  //  ARGPARSE_OPTS *arg_opts;
  const char *config;
};

typedef struct auth_method_s *auth_method_t;

#endif
