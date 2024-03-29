/*
 * Copyright 2008-2009 Katholieke Universiteit Leuven
 *
 * Use of this software is governed by the GNU LGPLv2.1 license
 *
 * Written by Sven Verdoolaege, K.U.Leuven, Departement
 * Computerwetenschappen, Celestijnenlaan 200A, B-3001 Leuven, Belgium
 */

#ifndef ISL_OPTIONS_H
#define ISL_OPTIONS_H

#include <isl/arg.h>
#include <isl/ctx.h>

#if defined(__cplusplus)
extern "C" {
#endif

struct isl_options;

ISL_ARG_DECL(isl_options, struct isl_options, isl_options_args)

#define			ISL_BOUND_BERNSTEIN	0
#define			ISL_BOUND_RANGE		1
int isl_options_set_bound(isl_ctx *ctx, int val);
int isl_options_get_bound(isl_ctx *ctx);

#define			ISL_ON_ERROR_WARN	0
#define			ISL_ON_ERROR_CONTINUE	1
#define			ISL_ON_ERROR_ABORT	2
int isl_options_set_on_error(isl_ctx *ctx, int val);
int isl_options_get_on_error(isl_ctx *ctx);

int isl_options_set_gbr_only_first(isl_ctx *ctx, int val);
int isl_options_get_gbr_only_first(isl_ctx *ctx);

#if defined(__cplusplus)
}
#endif

#endif
