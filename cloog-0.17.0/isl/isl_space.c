/*
 * Copyright 2008-2009 Katholieke Universiteit Leuven
 * Copyright 2010      INRIA Saclay
 *
 * Use of this software is governed by the GNU LGPLv2.1 license
 *
 * Written by Sven Verdoolaege, K.U.Leuven, Departement
 * Computerwetenschappen, Celestijnenlaan 200A, B-3001 Leuven, Belgium
 * and INRIA Saclay - Ile-de-France, Parc Club Orsay Universite,
 * ZAC des vignes, 4 rue Jacques Monod, 91893 Orsay, France 
 */

#include <stdlib.h>
#include <isl_space_private.h>
#include <isl_id_private.h>
#include <isl_reordering.h>

isl_ctx *isl_space_get_ctx(__isl_keep isl_space *dim)
{
	return dim ? dim->ctx : NULL;
}

__isl_give isl_space *isl_space_alloc(isl_ctx *ctx,
			unsigned nparam, unsigned n_in, unsigned n_out)
{
	isl_space *dim;

	dim = isl_alloc_type(ctx, struct isl_space);
	if (!dim)
		return NULL;

	dim->ctx = ctx;
	isl_ctx_ref(ctx);
	dim->ref = 1;
	dim->nparam = nparam;
	dim->n_in = n_in;
	dim->n_out = n_out;

	dim->tuple_id[0] = NULL;
	dim->tuple_id[1] = NULL;

	dim->nested[0] = NULL;
	dim->nested[1] = NULL;

	dim->n_id = 0;
	dim->ids = NULL;

	return dim;
}

/* Mark the space as being that of a set, by setting the domain tuple
 * to isl_id_none.
 */
static __isl_give isl_space *mark_as_set(__isl_take isl_space *space)
{
	space = isl_space_cow(space);
	if (!space)
		return NULL;
	space = isl_space_set_tuple_id(space, isl_dim_in, &isl_id_none);
	return space;
}

/* Is the space that of a set?
 */
int isl_space_is_set(__isl_keep isl_space *space)
{
	if (!space)
		return -1;
	if (space->n_in != 0 || space->nested[0])
		return 0;
	if (space->tuple_id[0] != &isl_id_none)
		return 0;
	return 1;
}

__isl_give isl_space *isl_space_set_alloc(isl_ctx *ctx,
			unsigned nparam, unsigned dim)
{
	isl_space *space;
	space = isl_space_alloc(ctx, nparam, 0, dim);
	space = mark_as_set(space);
	return space;
}

/* Mark the space as being that of a parameter domain, by setting
 * both tuples to isl_id_none.
 */
static __isl_give isl_space *mark_as_params(isl_space *space)
{
	if (!space)
		return NULL;
	space = isl_space_set_tuple_id(space, isl_dim_in, &isl_id_none);
	space = isl_space_set_tuple_id(space, isl_dim_out, &isl_id_none);
	return space;
}

/* Is the space that of a parameter domain?
 */
int isl_space_is_params(__isl_keep isl_space *space)
{
	if (!space)
		return -1;
	if (space->n_in != 0 || space->nested[0] ||
	    space->n_out != 0 || space->nested[1])
		return 0;
	if (space->tuple_id[0] != &isl_id_none)
		return 0;
	if (space->tuple_id[1] != &isl_id_none)
		return 0;
	return 1;
}

/* Create a space for a parameter domain.
 */
__isl_give isl_space *isl_space_params_alloc(isl_ctx *ctx, unsigned nparam)
{
	isl_space *space;
	space = isl_space_alloc(ctx, nparam, 0, 0);
	space = mark_as_params(space);
	return space;
}

static unsigned global_pos(__isl_keep isl_space *dim,
				 enum isl_dim_type type, unsigned pos)
{
	struct isl_ctx *ctx = dim->ctx;

	switch (type) {
	case isl_dim_param:
		isl_assert(ctx, pos < dim->nparam,
			    return isl_space_dim(dim, isl_dim_all));
		return pos;
	case isl_dim_in:
		isl_assert(ctx, pos < dim->n_in,
			    return isl_space_dim(dim, isl_dim_all));
		return pos + dim->nparam;
	case isl_dim_out:
		isl_assert(ctx, pos < dim->n_out,
			    return isl_space_dim(dim, isl_dim_all));
		return pos + dim->nparam + dim->n_in;
	default:
		isl_assert(ctx, 0, return isl_space_dim(dim, isl_dim_all));
	}
	return isl_space_dim(dim, isl_dim_all);
}

/* Extend length of ids array to the total number of dimensions.
 */
static __isl_give isl_space *extend_ids(__isl_take isl_space *dim)
{
	isl_id **ids;
	int i;

	if (isl_space_dim(dim, isl_dim_all) <= dim->n_id)
		return dim;

	if (!dim->ids) {
		dim->ids = isl_calloc_array(dim->ctx,
				isl_id *, isl_space_dim(dim, isl_dim_all));
		if (!dim->ids)
			goto error;
	} else {
		ids = isl_realloc_array(dim->ctx, dim->ids,
				isl_id *, isl_space_dim(dim, isl_dim_all));
		if (!ids)
			goto error;
		dim->ids = ids;
		for (i = dim->n_id; i < isl_space_dim(dim, isl_dim_all); ++i)
			dim->ids[i] = NULL;
	}

	dim->n_id = isl_space_dim(dim, isl_dim_all);

	return dim;
error:
	isl_space_free(dim);
	return NULL;
}

static __isl_give isl_space *set_id(__isl_take isl_space *dim,
	enum isl_dim_type type, unsigned pos, __isl_take isl_id *id)
{
	dim = isl_space_cow(dim);

	if (!dim)
		goto error;

	pos = global_pos(dim, type, pos);
	if (pos == isl_space_dim(dim, isl_dim_all))
		goto error;

	if (pos >= dim->n_id) {
		if (!id)
			return dim;
		dim = extend_ids(dim);
		if (!dim)
			goto error;
	}

	dim->ids[pos] = id;

	return dim;
error:
	isl_id_free(id);
	isl_space_free(dim);
	return NULL;
}

static __isl_keep isl_id *get_id(__isl_keep isl_space *dim,
				 enum isl_dim_type type, unsigned pos)
{
	if (!dim)
		return NULL;

	pos = global_pos(dim, type, pos);
	if (pos == isl_space_dim(dim, isl_dim_all))
		return NULL;
	if (pos >= dim->n_id)
		return NULL;
	return dim->ids[pos];
}

static unsigned offset(__isl_keep isl_space *dim, enum isl_dim_type type)
{
	switch (type) {
	case isl_dim_param:	return 0;
	case isl_dim_in:	return dim->nparam;
	case isl_dim_out:	return dim->nparam + dim->n_in;
	default:		return 0;
	}
}

static unsigned n(__isl_keep isl_space *dim, enum isl_dim_type type)
{
	switch (type) {
	case isl_dim_param:	return dim->nparam;
	case isl_dim_in:	return dim->n_in;
	case isl_dim_out:	return dim->n_out;
	case isl_dim_all:	return dim->nparam + dim->n_in + dim->n_out;
	default:		return 0;
	}
}

unsigned isl_space_dim(__isl_keep isl_space *dim, enum isl_dim_type type)
{
	if (!dim)
		return 0;
	return n(dim, type);
}

unsigned isl_space_offset(__isl_keep isl_space *dim, enum isl_dim_type type)
{
	if (!dim)
		return 0;
	return offset(dim, type);
}

static __isl_give isl_space *copy_ids(__isl_take isl_space *dst,
	enum isl_dim_type dst_type, unsigned offset, __isl_keep isl_space *src,
	enum isl_dim_type src_type)
{
	int i;
	isl_id *id;

	if (!dst)
		return NULL;

	for (i = 0; i < n(src, src_type); ++i) {
		id = get_id(src, src_type, i);
		if (!id)
			continue;
		dst = set_id(dst, dst_type, offset + i, isl_id_copy(id));
		if (!dst)
			return NULL;
	}
	return dst;
}

__isl_take isl_space *isl_space_dup(__isl_keep isl_space *dim)
{
	isl_space *dup;
	if (!dim)
		return NULL;
	dup = isl_space_alloc(dim->ctx, dim->nparam, dim->n_in, dim->n_out);
	if (dim->tuple_id[0] &&
	    !(dup->tuple_id[0] = isl_id_copy(dim->tuple_id[0])))
		goto error;
	if (dim->tuple_id[1] &&
	    !(dup->tuple_id[1] = isl_id_copy(dim->tuple_id[1])))
		goto error;
	if (dim->nested[0] && !(dup->nested[0] = isl_space_copy(dim->nested[0])))
		goto error;
	if (dim->nested[1] && !(dup->nested[1] = isl_space_copy(dim->nested[1])))
		goto error;
	if (!dim->ids)
		return dup;
	dup = copy_ids(dup, isl_dim_param, 0, dim, isl_dim_param);
	dup = copy_ids(dup, isl_dim_in, 0, dim, isl_dim_in);
	dup = copy_ids(dup, isl_dim_out, 0, dim, isl_dim_out);
	return dup;
error:
	isl_space_free(dup);
	return NULL;
}

__isl_give isl_space *isl_space_cow(__isl_take isl_space *dim)
{
	if (!dim)
		return NULL;

	if (dim->ref == 1)
		return dim;
	dim->ref--;
	return isl_space_dup(dim);
}

__isl_give isl_space *isl_space_copy(__isl_keep isl_space *dim)
{
	if (!dim)
		return NULL;

	dim->ref++;
	return dim;
}

void isl_space_free(__isl_take isl_space *dim)
{
	int i;

	if (!dim)
		return;

	if (--dim->ref > 0)
		return;

	isl_id_free(dim->tuple_id[0]);
	isl_id_free(dim->tuple_id[1]);

	isl_space_free(dim->nested[0]);
	isl_space_free(dim->nested[1]);

	for (i = 0; i < dim->n_id; ++i)
		isl_id_free(dim->ids[i]);
	free(dim->ids);
	isl_ctx_deref(dim->ctx);
	
	free(dim);
}

static int name_ok(isl_ctx *ctx, const char *s)
{
	char *p;
	long dummy;

	dummy = strtol(s, &p, 0);
	if (p != s)
		isl_die(ctx, isl_error_invalid, "name looks like a number",
			return 0);

	return 1;
}

int isl_space_has_tuple_id(__isl_keep isl_space *dim, enum isl_dim_type type)
{
	if (!dim)
		return -1;
	if (isl_space_is_params(dim))
		isl_die(dim->ctx, isl_error_invalid,
			"parameter spaces don't have tuple ids", return -1);
	if (isl_space_is_set(dim) && type != isl_dim_set)
		isl_die(dim->ctx, isl_error_invalid,
			"set spaces can only have a set id", return -1);
	if (type != isl_dim_in && type != isl_dim_out)
		isl_die(dim->ctx, isl_error_invalid,
			"only input, output and set tuples can have ids",
			return -1);
	return dim->tuple_id[type - isl_dim_in] != NULL;
}

__isl_give isl_id *isl_space_get_tuple_id(__isl_keep isl_space *dim,
	enum isl_dim_type type)
{
	int has_id;

	if (!dim)
		return NULL;
	has_id = isl_space_has_tuple_id(dim, type);
	if (has_id < 0)
		return NULL;
	if (!has_id)
		isl_die(dim->ctx, isl_error_invalid,
			"tuple has no id", return NULL);
	return isl_id_copy(dim->tuple_id[type - isl_dim_in]);
}

__isl_give isl_space *isl_space_set_tuple_id(__isl_take isl_space *dim,
	enum isl_dim_type type, __isl_take isl_id *id)
{
	dim = isl_space_cow(dim);
	if (!dim || !id)
		goto error;
	if (type != isl_dim_in && type != isl_dim_out)
		isl_die(dim->ctx, isl_error_invalid,
			"only input, output and set tuples can have names",
			goto error);

	isl_id_free(dim->tuple_id[type - isl_dim_in]);
	dim->tuple_id[type - isl_dim_in] = id;

	return dim;
error:
	isl_id_free(id);
	isl_space_free(dim);
	return NULL;
}

__isl_give isl_space *isl_space_reset_tuple_id(__isl_take isl_space *dim,
	enum isl_dim_type type)
{
	dim = isl_space_cow(dim);
	if (!dim)
		return NULL;
	if (type != isl_dim_in && type != isl_dim_out)
		isl_die(dim->ctx, isl_error_invalid,
			"only input, output and set tuples can have names",
			goto error);

	isl_id_free(dim->tuple_id[type - isl_dim_in]);
	dim->tuple_id[type - isl_dim_in] = NULL;

	return dim;
error:
	isl_space_free(dim);
	return NULL;
}

__isl_give isl_space *isl_space_set_dim_id(__isl_take isl_space *dim,
	enum isl_dim_type type, unsigned pos, __isl_take isl_id *id)
{
	dim = isl_space_cow(dim);
	if (!dim || !id)
		goto error;
	isl_id_free(get_id(dim, type, pos));
	return set_id(dim, type, pos, id);
error:
	isl_id_free(id);
	isl_space_free(dim);
	return NULL;
}

int isl_space_has_dim_id(__isl_keep isl_space *dim,
	enum isl_dim_type type, unsigned pos)
{
	if (!dim)
		return -1;
	return get_id(dim, type, pos) != NULL;
}

__isl_give isl_id *isl_space_get_dim_id(__isl_keep isl_space *dim,
	enum isl_dim_type type, unsigned pos)
{
	if (!dim)
		return NULL;
	if (!get_id(dim, type, pos))
		isl_die(dim->ctx, isl_error_invalid,
			"dim has no id", return NULL);
	return isl_id_copy(get_id(dim, type, pos));
}

__isl_give isl_space *isl_space_set_tuple_name(__isl_take isl_space *dim,
	enum isl_dim_type type, const char *s)
{
	isl_id *id;

	if (!dim)
		return NULL;

	if (!s)
		return isl_space_reset_tuple_id(dim, type);

	if (!name_ok(dim->ctx, s))
		goto error;

	id = isl_id_alloc(dim->ctx, s, NULL);
	return isl_space_set_tuple_id(dim, type, id);
error:
	isl_space_free(dim);
	return NULL;
}

const char *isl_space_get_tuple_name(__isl_keep isl_space *dim,
	 enum isl_dim_type type)
{
	isl_id *id;
	if (!dim)
		return NULL;
	if (type != isl_dim_in && type != isl_dim_out)
		return NULL;
	id = dim->tuple_id[type - isl_dim_in];
	return id ? id->name : NULL;
}

__isl_give isl_space *isl_space_set_dim_name(__isl_take isl_space *dim,
				 enum isl_dim_type type, unsigned pos,
				 const char *s)
{
	isl_id *id;

	if (!dim)
		return NULL;
	if (!name_ok(dim->ctx, s))
		goto error;
	id = isl_id_alloc(dim->ctx, s, NULL);
	return isl_space_set_dim_id(dim, type, pos, id);
error:
	isl_space_free(dim);
	return NULL;
}

__isl_keep const char *isl_space_get_dim_name(__isl_keep isl_space *dim,
				 enum isl_dim_type type, unsigned pos)
{
	isl_id *id = get_id(dim, type, pos);
	return id ? id->name : NULL;
}

int isl_space_find_dim_by_id(__isl_keep isl_space *dim, enum isl_dim_type type,
	__isl_keep isl_id *id)
{
	int i;
	int offset;
	int n;

	if (!dim || !id)
		return -1;

	offset = isl_space_offset(dim, type);
	n = isl_space_dim(dim, type);
	for (i = 0; i < n && offset + i < dim->n_id; ++i)
		if (dim->ids[offset + i] == id)
			return i;

	return -1;
}

int isl_space_find_dim_by_name(__isl_keep isl_space *space,
	enum isl_dim_type type, const char *name)
{
	int i;
	int offset;
	int n;

	if (!space || !name)
		return -1;

	offset = isl_space_offset(space, type);
	n = isl_space_dim(space, type);
	for (i = 0; i < n && offset + i < space->n_id; ++i)
		if (space->ids[offset + i]->name &&
		    !strcmp(space->ids[offset + i]->name, name))
			return i;

	return -1;
}

static __isl_keep isl_id *tuple_id(__isl_keep isl_space *dim,
	enum isl_dim_type type)
{
	if (!dim)
		return NULL;
	if (type == isl_dim_in)
		return dim->tuple_id[0];
	if (type == isl_dim_out)
		return dim->tuple_id[1];
	return NULL;
}

static __isl_keep isl_space *nested(__isl_keep isl_space *dim,
	enum isl_dim_type type)
{
	if (!dim)
		return NULL;
	if (type == isl_dim_in)
		return dim->nested[0];
	if (type == isl_dim_out)
		return dim->nested[1];
	return NULL;
}

int isl_space_tuple_match(__isl_keep isl_space *dim1, enum isl_dim_type dim1_type,
			__isl_keep isl_space *dim2, enum isl_dim_type dim2_type)
{
	isl_id *id1, *id2;
	isl_space *nested1, *nested2;

	if (!dim1 || !dim2)
		return -1;

	if (dim1 == dim2 && dim1_type == dim2_type)
		return 1;

	if (n(dim1, dim1_type) != n(dim2, dim2_type))
		return 0;
	id1 = tuple_id(dim1, dim1_type);
	id2 = tuple_id(dim2, dim2_type);
	if (!id1 ^ !id2)
		return 0;
	if (id1 && id1 != id2)
		return 0;
	nested1 = nested(dim1, dim1_type);
	nested2 = nested(dim2, dim2_type);
	if (!nested1 ^ !nested2)
		return 0;
	if (nested1 && !isl_space_is_equal(nested1, nested2))
		return 0;
	return 1;
}

static int match(__isl_keep isl_space *dim1, enum isl_dim_type dim1_type,
	__isl_keep isl_space *dim2, enum isl_dim_type dim2_type)
{
	int i;

	if (dim1 == dim2 && dim1_type == dim2_type)
		return 1;

	if (!isl_space_tuple_match(dim1, dim1_type, dim2, dim2_type))
		return 0;

	if (!dim1->ids && !dim2->ids)
		return 1;

	for (i = 0; i < n(dim1, dim1_type); ++i) {
		if (get_id(dim1, dim1_type, i) != get_id(dim2, dim2_type, i))
			return 0;
	}
	return 1;
}

int isl_space_match(__isl_keep isl_space *dim1, enum isl_dim_type dim1_type,
	__isl_keep isl_space *dim2, enum isl_dim_type dim2_type)
{
	if (!dim1 || !dim2)
		return -1;

	return match(dim1, dim1_type, dim2, dim2_type);
}

static void get_ids(__isl_keep isl_space *dim, enum isl_dim_type type,
	unsigned first, unsigned n, __isl_keep isl_id **ids)
{
	int i;

	for (i = 0; i < n ; ++i)
		ids[i] = get_id(dim, type, first + i);
}

__isl_give isl_space *isl_space_extend(__isl_take isl_space *dim,
			unsigned nparam, unsigned n_in, unsigned n_out)
{
	isl_id **ids = NULL;

	if (!dim)
		return NULL;
	if (dim->nparam == nparam && dim->n_in == n_in && dim->n_out == n_out)
		return dim;

	isl_assert(dim->ctx, dim->nparam <= nparam, goto error);
	isl_assert(dim->ctx, dim->n_in <= n_in, goto error);
	isl_assert(dim->ctx, dim->n_out <= n_out, goto error);

	dim = isl_space_cow(dim);

	if (dim->ids) {
		ids = isl_calloc_array(dim->ctx, isl_id *,
					 nparam + n_in + n_out);
		if (!ids)
			goto error;
		get_ids(dim, isl_dim_param, 0, dim->nparam, ids);
		get_ids(dim, isl_dim_in, 0, dim->n_in, ids + nparam);
		get_ids(dim, isl_dim_out, 0, dim->n_out, ids + nparam + n_in);
		free(dim->ids);
		dim->ids = ids;
		dim->n_id = nparam + n_in + n_out;
	}
	dim->nparam = nparam;
	dim->n_in = n_in;
	dim->n_out = n_out;

	return dim;
error:
	free(ids);
	isl_space_free(dim);
	return NULL;
}

__isl_give isl_space *isl_space_add_dims(__isl_take isl_space *dim,
	enum isl_dim_type type, unsigned n)
{
	if (!dim)
		return NULL;
	dim = isl_space_reset(dim, type);
	switch (type) {
	case isl_dim_param:
		dim = isl_space_extend(dim,
					dim->nparam + n, dim->n_in, dim->n_out);
		if (dim && dim->nested[0] &&
		    !(dim->nested[0] = isl_space_add_dims(dim->nested[0],
						    isl_dim_param, n)))
			goto error;
		if (dim && dim->nested[1] &&
		    !(dim->nested[1] = isl_space_add_dims(dim->nested[1],
						    isl_dim_param, n)))
			goto error;
		return dim;
	case isl_dim_in:
		return isl_space_extend(dim,
					dim->nparam, dim->n_in + n, dim->n_out);
	case isl_dim_out:
		return isl_space_extend(dim,
					dim->nparam, dim->n_in, dim->n_out + n);
	default:
		isl_die(dim->ctx, isl_error_invalid,
			"cannot add dimensions of specified type", goto error);
	}
error:
	isl_space_free(dim);
	return NULL;
}

static int valid_dim_type(enum isl_dim_type type)
{
	switch (type) {
	case isl_dim_param:
	case isl_dim_in:
	case isl_dim_out:
		return 1;
	default:
		return 0;
	}
}

__isl_give isl_space *isl_space_insert_dims(__isl_take isl_space *dim,
	enum isl_dim_type type, unsigned pos, unsigned n)
{
	isl_id **ids = NULL;

	if (!dim)
		return NULL;
	if (n == 0)
		return isl_space_reset(dim, type);

	if (!valid_dim_type(type))
		isl_die(dim->ctx, isl_error_invalid,
			"cannot insert dimensions of specified type",
			goto error);

	isl_assert(dim->ctx, pos <= isl_space_dim(dim, type), goto error);

	dim = isl_space_cow(dim);
	if (!dim)
		return NULL;

	if (dim->ids) {
		enum isl_dim_type t;
		int off;
		int s[3];
		int *size = s - isl_dim_param;
		ids = isl_calloc_array(dim->ctx, isl_id *,
				     dim->nparam + dim->n_in + dim->n_out + n);
		if (!ids)
			goto error;
		off = 0;
		size[isl_dim_param] = dim->nparam;
		size[isl_dim_in] = dim->n_in;
		size[isl_dim_out] = dim->n_out;
		for (t = isl_dim_param; t <= isl_dim_out; ++t) {
			if (t != type) {
				get_ids(dim, t, 0, size[t], ids + off);
				off += size[t];
			} else {
				get_ids(dim, t, 0, pos, ids + off);
				off += pos + n;
				get_ids(dim, t, pos, size[t] - pos, ids + off);
				off += size[t] - pos;
			}
		}
		free(dim->ids);
		dim->ids = ids;
		dim->n_id = dim->nparam + dim->n_in + dim->n_out + n;
	}
	switch (type) {
	case isl_dim_param:	dim->nparam += n; break;
	case isl_dim_in:	dim->n_in += n; break;
	case isl_dim_out:	dim->n_out += n; break;
	default:		;
	}
	dim = isl_space_reset(dim, type);

	return dim;
error:
	isl_space_free(dim);
	return NULL;
}

__isl_give isl_space *isl_space_move_dims(__isl_take isl_space *dim,
	enum isl_dim_type dst_type, unsigned dst_pos,
	enum isl_dim_type src_type, unsigned src_pos, unsigned n)
{
	int i;

	if (!dim)
		return NULL;
	if (n == 0)
		return dim;

	isl_assert(dim->ctx, src_pos + n <= isl_space_dim(dim, src_type),
		goto error);

	if (dst_type == src_type && dst_pos == src_pos)
		return dim;

	isl_assert(dim->ctx, dst_type != src_type, goto error);

	dim = isl_space_reset(dim, src_type);
	dim = isl_space_reset(dim, dst_type);

	dim = isl_space_cow(dim);
	if (!dim)
		return NULL;

	if (dim->ids) {
		isl_id **ids;
		enum isl_dim_type t;
		int off;
		int s[3];
		int *size = s - isl_dim_param;
		ids = isl_calloc_array(dim->ctx, isl_id *,
					 dim->nparam + dim->n_in + dim->n_out);
		if (!ids)
			goto error;
		off = 0;
		size[isl_dim_param] = dim->nparam;
		size[isl_dim_in] = dim->n_in;
		size[isl_dim_out] = dim->n_out;
		for (t = isl_dim_param; t <= isl_dim_out; ++t) {
			if (t == dst_type) {
				get_ids(dim, t, 0, dst_pos, ids + off);
				off += dst_pos;
				get_ids(dim, src_type, src_pos, n, ids + off);
				off += n;
				get_ids(dim, t, dst_pos, size[t] - dst_pos,
						ids + off);
				off += size[t] - dst_pos;
			} else if (t == src_type) {
				get_ids(dim, t, 0, src_pos, ids + off);
				off += src_pos;
				get_ids(dim, t, src_pos + n,
					    size[t] - src_pos - n, ids + off);
				off += size[t] - src_pos - n;
			} else {
				get_ids(dim, t, 0, size[t], ids + off);
				off += size[t];
			}
		}
		free(dim->ids);
		dim->ids = ids;
		dim->n_id = dim->nparam + dim->n_in + dim->n_out;
	}

	switch (dst_type) {
	case isl_dim_param:	dim->nparam += n; break;
	case isl_dim_in:	dim->n_in += n; break;
	case isl_dim_out:	dim->n_out += n; break;
	default:		;
	}

	switch (src_type) {
	case isl_dim_param:	dim->nparam -= n; break;
	case isl_dim_in:	dim->n_in -= n; break;
	case isl_dim_out:	dim->n_out -= n; break;
	default:		;
	}

	if (dst_type != isl_dim_param && src_type != isl_dim_param)
		return dim;

	for (i = 0; i < 2; ++i) {
		if (!dim->nested[i])
			continue;
		dim->nested[i] = isl_space_replace(dim->nested[i],
						 isl_dim_param, dim);
		if (!dim->nested[i])
			goto error;
	}

	return dim;
error:
	isl_space_free(dim);
	return NULL;
}

__isl_give isl_space *isl_space_join(__isl_take isl_space *left,
	__isl_take isl_space *right)
{
	isl_space *dim;

	if (!left || !right)
		goto error;

	isl_assert(left->ctx, match(left, isl_dim_param, right, isl_dim_param),
			goto error);
	isl_assert(left->ctx,
		isl_space_tuple_match(left, isl_dim_out, right, isl_dim_in),
		goto error);

	dim = isl_space_alloc(left->ctx, left->nparam, left->n_in, right->n_out);
	if (!dim)
		goto error;

	dim = copy_ids(dim, isl_dim_param, 0, left, isl_dim_param);
	dim = copy_ids(dim, isl_dim_in, 0, left, isl_dim_in);
	dim = copy_ids(dim, isl_dim_out, 0, right, isl_dim_out);

	if (dim && left->tuple_id[0] &&
	    !(dim->tuple_id[0] = isl_id_copy(left->tuple_id[0])))
		goto error;
	if (dim && right->tuple_id[1] &&
	    !(dim->tuple_id[1] = isl_id_copy(right->tuple_id[1])))
		goto error;
	if (dim && left->nested[0] &&
	    !(dim->nested[0] = isl_space_copy(left->nested[0])))
		goto error;
	if (dim && right->nested[1] &&
	    !(dim->nested[1] = isl_space_copy(right->nested[1])))
		goto error;

	isl_space_free(left);
	isl_space_free(right);

	return dim;
error:
	isl_space_free(left);
	isl_space_free(right);
	return NULL;
}

__isl_give isl_space *isl_space_product(__isl_take isl_space *left,
	__isl_take isl_space *right)
{
	isl_space *dom1, *dom2, *nest1, *nest2;

	if (!left || !right)
		goto error;

	isl_assert(left->ctx, match(left, isl_dim_param, right, isl_dim_param),
			goto error);

	dom1 = isl_space_domain(isl_space_copy(left));
	dom2 = isl_space_domain(isl_space_copy(right));
	nest1 = isl_space_wrap(isl_space_join(isl_space_reverse(dom1), dom2));

	dom1 = isl_space_range(left);
	dom2 = isl_space_range(right);
	nest2 = isl_space_wrap(isl_space_join(isl_space_reverse(dom1), dom2));

	return isl_space_join(isl_space_reverse(nest1), nest2);
error:
	isl_space_free(left);
	isl_space_free(right);
	return NULL;
}

/* Given two spaces { A -> C } and { B -> C }, construct the space
 * { [A -> B] -> C }
 */
__isl_give isl_space *isl_space_domain_product(__isl_take isl_space *left,
	__isl_take isl_space *right)
{
	isl_space *ran, *dom1, *dom2, *nest;

	if (!left || !right)
		goto error;

	if (!match(left, isl_dim_param, right, isl_dim_param))
		isl_die(left->ctx, isl_error_invalid,
			"parameters need to match", goto error);
	if (!isl_space_tuple_match(left, isl_dim_out, right, isl_dim_out))
		isl_die(left->ctx, isl_error_invalid,
			"ranges need to match", goto error);

	ran = isl_space_range(isl_space_copy(left));

	dom1 = isl_space_domain(left);
	dom2 = isl_space_domain(right);
	nest = isl_space_wrap(isl_space_join(isl_space_reverse(dom1), dom2));

	return isl_space_join(isl_space_reverse(nest), ran);
error:
	isl_space_free(left);
	isl_space_free(right);
	return NULL;
}

__isl_give isl_space *isl_space_range_product(__isl_take isl_space *left,
	__isl_take isl_space *right)
{
	isl_space *dom, *ran1, *ran2, *nest;

	if (!left || !right)
		goto error;

	isl_assert(left->ctx, match(left, isl_dim_param, right, isl_dim_param),
			goto error);
	if (!isl_space_tuple_match(left, isl_dim_in, right, isl_dim_in))
		isl_die(left->ctx, isl_error_invalid,
			"domains need to match", goto error);

	dom = isl_space_domain(isl_space_copy(left));

	ran1 = isl_space_range(left);
	ran2 = isl_space_range(right);
	nest = isl_space_wrap(isl_space_join(isl_space_reverse(ran1), ran2));

	return isl_space_join(isl_space_reverse(dom), nest);
error:
	isl_space_free(left);
	isl_space_free(right);
	return NULL;
}

__isl_give isl_space *isl_space_map_from_set(__isl_take isl_space *dim)
{
	isl_ctx *ctx;
	isl_id **ids = NULL;

	if (!dim)
		return NULL;
	ctx = isl_space_get_ctx(dim);
	if (!isl_space_is_set(dim))
		isl_die(ctx, isl_error_invalid, "not a set space", goto error);
	dim = isl_space_cow(dim);
	if (!dim)
		return NULL;
	if (dim->ids) {
		ids = isl_calloc_array(dim->ctx, isl_id *,
					dim->nparam + dim->n_out + dim->n_out);
		if (!ids)
			goto error;
		get_ids(dim, isl_dim_param, 0, dim->nparam, ids);
		get_ids(dim, isl_dim_out, 0, dim->n_out, ids + dim->nparam);
	}
	dim->n_in = dim->n_out;
	if (ids) {
		free(dim->ids);
		dim->ids = ids;
		dim->n_id = dim->nparam + dim->n_out + dim->n_out;
		dim = copy_ids(dim, isl_dim_out, 0, dim, isl_dim_in);
	}
	isl_id_free(dim->tuple_id[0]);
	dim->tuple_id[0] = isl_id_copy(dim->tuple_id[1]);
	isl_space_free(dim->nested[0]);
	dim->nested[0] = isl_space_copy(dim->nested[1]);
	return dim;
error:
	isl_space_free(dim);
	return NULL;
}

static __isl_give isl_space *set_ids(__isl_take isl_space *dim,
	enum isl_dim_type type,
	unsigned first, unsigned n, __isl_take isl_id **ids)
{
	int i;

	for (i = 0; i < n ; ++i)
		dim = set_id(dim, type, first + i, ids[i]);

	return dim;
}

__isl_give isl_space *isl_space_reverse(__isl_take isl_space *dim)
{
	unsigned t;
	isl_space *nested;
	isl_id **ids = NULL;
	isl_id *id;

	if (!dim)
		return NULL;
	if (match(dim, isl_dim_in, dim, isl_dim_out))
		return dim;

	dim = isl_space_cow(dim);
	if (!dim)
		return NULL;

	id = dim->tuple_id[0];
	dim->tuple_id[0] = dim->tuple_id[1];
	dim->tuple_id[1] = id;

	nested = dim->nested[0];
	dim->nested[0] = dim->nested[1];
	dim->nested[1] = nested;

	if (dim->ids) {
		ids = isl_alloc_array(dim->ctx, isl_id *,
					dim->n_in + dim->n_out);
		if (!ids)
			goto error;
		get_ids(dim, isl_dim_in, 0, dim->n_in, ids);
		get_ids(dim, isl_dim_out, 0, dim->n_out, ids + dim->n_in);
	}

	t = dim->n_in;
	dim->n_in = dim->n_out;
	dim->n_out = t;

	if (dim->ids) {
		dim = set_ids(dim, isl_dim_out, 0, dim->n_out, ids);
		dim = set_ids(dim, isl_dim_in, 0, dim->n_in, ids + dim->n_out);
		free(ids);
	}

	return dim;
error:
	free(ids);
	isl_space_free(dim);
	return NULL;
}

__isl_give isl_space *isl_space_drop_dims(__isl_take isl_space *dim,
	enum isl_dim_type type, unsigned first, unsigned num)
{
	int i;

	if (!dim)
		return NULL;

	if (num == 0)
		return isl_space_reset(dim, type);

	if (!valid_dim_type(type))
		isl_die(dim->ctx, isl_error_invalid,
			"cannot drop dimensions of specified type", goto error);

	isl_assert(dim->ctx, first + num <= n(dim, type), goto error);
	dim = isl_space_cow(dim);
	if (!dim)
		goto error;
	if (dim->ids) {
		dim = extend_ids(dim);
		if (!dim)
			goto error;
		for (i = 0; i < num; ++i)
			isl_id_free(get_id(dim, type, first + i));
		for (i = first+num; i < n(dim, type); ++i)
			set_id(dim, type, i - num, get_id(dim, type, i));
		switch (type) {
		case isl_dim_param:
			get_ids(dim, isl_dim_in, 0, dim->n_in,
				dim->ids + offset(dim, isl_dim_in) - num);
		case isl_dim_in:
			get_ids(dim, isl_dim_out, 0, dim->n_out,
				dim->ids + offset(dim, isl_dim_out) - num);
		default:
			;
		}
		dim->n_id -= num;
	}
	switch (type) {
	case isl_dim_param:	dim->nparam -= num; break;
	case isl_dim_in:	dim->n_in -= num; break;
	case isl_dim_out:	dim->n_out -= num; break;
	default:		;
	}
	dim = isl_space_reset(dim, type);
	if (type == isl_dim_param) {
		if (dim && dim->nested[0] &&
		    !(dim->nested[0] = isl_space_drop_dims(dim->nested[0],
						    isl_dim_param, first, num)))
			goto error;
		if (dim && dim->nested[1] &&
		    !(dim->nested[1] = isl_space_drop_dims(dim->nested[1],
						    isl_dim_param, first, num)))
			goto error;
	}
	return dim;
error:
	isl_space_free(dim);
	return NULL;
}

__isl_give isl_space *isl_space_drop_inputs(__isl_take isl_space *dim,
		unsigned first, unsigned n)
{
	if (!dim)
		return NULL;
	return isl_space_drop_dims(dim, isl_dim_in, first, n);
}

__isl_give isl_space *isl_space_drop_outputs(__isl_take isl_space *dim,
		unsigned first, unsigned n)
{
	if (!dim)
		return NULL;
	return isl_space_drop_dims(dim, isl_dim_out, first, n);
}

__isl_give isl_space *isl_space_domain(__isl_take isl_space *dim)
{
	if (!dim)
		return NULL;
	dim = isl_space_drop_outputs(dim, 0, dim->n_out);
	dim = isl_space_reverse(dim);
	dim = mark_as_set(dim);
	return dim;
}

__isl_give isl_space *isl_space_from_domain(__isl_take isl_space *dim)
{
	if (!dim)
		return NULL;
	if (!isl_space_is_set(dim))
		isl_die(isl_space_get_ctx(dim), isl_error_invalid,
			"not a set space", goto error);
	dim = isl_space_reverse(dim);
	dim = isl_space_reset(dim, isl_dim_out);
	return dim;
error:
	isl_space_free(dim);
	return NULL;
}

__isl_give isl_space *isl_space_range(__isl_take isl_space *dim)
{
	if (!dim)
		return NULL;
	dim = isl_space_drop_inputs(dim, 0, dim->n_in);
	dim = mark_as_set(dim);
	return dim;
}

__isl_give isl_space *isl_space_from_range(__isl_take isl_space *dim)
{
	if (!dim)
		return NULL;
	if (!isl_space_is_set(dim))
		isl_die(isl_space_get_ctx(dim), isl_error_invalid,
			"not a set space", goto error);
	return isl_space_reset(dim, isl_dim_in);
error:
	isl_space_free(dim);
	return NULL;
}

__isl_give isl_space *isl_space_params(__isl_take isl_space *space)
{
	if (isl_space_is_params(space))
		return space;
	space = isl_space_drop_dims(space,
			    isl_dim_in, 0, isl_space_dim(space, isl_dim_in));
	space = isl_space_drop_dims(space,
			    isl_dim_out, 0, isl_space_dim(space, isl_dim_out));
	space = mark_as_params(space);
	return space;
}

__isl_give isl_space *isl_space_set_from_params(__isl_take isl_space *space)
{
	if (!space)
		return NULL;
	if (!isl_space_is_params(space))
		isl_die(isl_space_get_ctx(space), isl_error_invalid,
			"not a parameter space", goto error);
	return isl_space_reset(space, isl_dim_set);
error:
	isl_space_free(space);
	return NULL;
}

__isl_give isl_space *isl_space_as_set_space(__isl_take isl_space *dim)
{
	dim = isl_space_cow(dim);
	if (!dim)
		return NULL;

	dim->n_out += dim->n_in;
	dim->n_in = 0;
	dim = isl_space_reset(dim, isl_dim_in);
	dim = isl_space_reset(dim, isl_dim_out);

	return dim;
}

__isl_give isl_space *isl_space_underlying(__isl_take isl_space *dim,
	unsigned n_div)
{
	int i;

	if (!dim)
		return NULL;
	if (n_div == 0 &&
	    dim->nparam == 0 && dim->n_in == 0 && dim->n_id == 0)
		return isl_space_reset(isl_space_reset(dim, isl_dim_in), isl_dim_out);
	dim = isl_space_cow(dim);
	if (!dim)
		return NULL;
	dim->n_out += dim->nparam + dim->n_in + n_div;
	dim->nparam = 0;
	dim->n_in = 0;

	for (i = 0; i < dim->n_id; ++i)
		isl_id_free(get_id(dim, isl_dim_out, i));
	dim->n_id = 0;
	dim = isl_space_reset(dim, isl_dim_in);
	dim = isl_space_reset(dim, isl_dim_out);

	return dim;
}

int isl_space_is_equal(__isl_keep isl_space *dim1, __isl_keep isl_space *dim2)
{
	if (!dim1 || !dim2)
		return -1;
	if (dim1 == dim2)
		return 1;
	return match(dim1, isl_dim_param, dim2, isl_dim_param) &&
	       isl_space_tuple_match(dim1, isl_dim_in, dim2, isl_dim_in) &&
	       isl_space_tuple_match(dim1, isl_dim_out, dim2, isl_dim_out);
}

/* Is space1 equal to the domain of space2?
 */
int isl_space_is_domain(__isl_keep isl_space *space1,
	__isl_keep isl_space *space2)
{
	if (!space1 || !space2)
		return -1;
	if (!isl_space_is_set(space1))
		return 0;
	return match(space1, isl_dim_param, space2, isl_dim_param) &&
	       isl_space_tuple_match(space1, isl_dim_set, space2, isl_dim_in);
}

int isl_space_compatible(__isl_keep isl_space *dim1,
	__isl_keep isl_space *dim2)
{
	return dim1->nparam == dim2->nparam &&
	       dim1->n_in + dim1->n_out == dim2->n_in + dim2->n_out;
}

static uint32_t isl_hash_dim(uint32_t hash, __isl_keep isl_space *dim)
{
	int i;
	isl_id *id;

	if (!dim)
		return hash;

	hash = isl_hash_builtin(hash, dim->nparam);
	hash = isl_hash_builtin(hash, dim->n_in);
	hash = isl_hash_builtin(hash, dim->n_out);

	for (i = 0; i < dim->nparam; ++i) {
		id = get_id(dim, isl_dim_param, i);
		hash = isl_hash_id(hash, id);
	}

	id = tuple_id(dim, isl_dim_in);
	hash = isl_hash_id(hash, id);
	id = tuple_id(dim, isl_dim_out);
	hash = isl_hash_id(hash, id);

	hash = isl_hash_dim(hash, dim->nested[0]);
	hash = isl_hash_dim(hash, dim->nested[1]);

	return hash;
}

uint32_t isl_space_get_hash(__isl_keep isl_space *dim)
{
	uint32_t hash;

	if (!dim)
		return 0;

	hash = isl_hash_init();
	hash = isl_hash_dim(hash, dim);

	return hash;
}

int isl_space_is_wrapping(__isl_keep isl_space *dim)
{
	if (!dim)
		return -1;

	if (!isl_space_is_set(dim))
		return 0;

	return dim->nested[1] != NULL;
}

__isl_give isl_space *isl_space_wrap(__isl_take isl_space *dim)
{
	isl_space *wrap;

	if (!dim)
		return NULL;

	wrap = isl_space_set_alloc(dim->ctx,
				    dim->nparam, dim->n_in + dim->n_out);

	wrap = copy_ids(wrap, isl_dim_param, 0, dim, isl_dim_param);
	wrap = copy_ids(wrap, isl_dim_set, 0, dim, isl_dim_in);
	wrap = copy_ids(wrap, isl_dim_set, dim->n_in, dim, isl_dim_out);

	if (!wrap)
		goto error;

	wrap->nested[1] = dim;

	return wrap;
error:
	isl_space_free(dim);
	return NULL;
}

__isl_give isl_space *isl_space_unwrap(__isl_take isl_space *dim)
{
	isl_space *unwrap;

	if (!dim)
		return NULL;

	if (!isl_space_is_wrapping(dim))
		isl_die(dim->ctx, isl_error_invalid, "not a wrapping dim",
			goto error);

	unwrap = isl_space_copy(dim->nested[1]);
	isl_space_free(dim);

	return unwrap;
error:
	isl_space_free(dim);
	return NULL;
}

int isl_space_is_named_or_nested(__isl_keep isl_space *dim, enum isl_dim_type type)
{
	if (type != isl_dim_in && type != isl_dim_out)
		return 0;
	if (!dim)
		return -1;
	if (dim->tuple_id[type - isl_dim_in])
		return 1;
	if (dim->nested[type - isl_dim_in])
		return 1;
	return 0;
}

int isl_space_may_be_set(__isl_keep isl_space *dim)
{
	if (!dim)
		return -1;
	if (isl_space_is_set(dim))
		return 1;
	if (isl_space_dim(dim, isl_dim_in) != 0)
		return 0;
	if (isl_space_is_named_or_nested(dim, isl_dim_in))
		return 0;
	return 1;
}

__isl_give isl_space *isl_space_reset(__isl_take isl_space *dim,
	enum isl_dim_type type)
{
	if (!isl_space_is_named_or_nested(dim, type))
		return dim;

	dim = isl_space_cow(dim);
	if (!dim)
		return NULL;

	isl_id_free(dim->tuple_id[type - isl_dim_in]);
	dim->tuple_id[type - isl_dim_in] = NULL;
	isl_space_free(dim->nested[type - isl_dim_in]);
	dim->nested[type - isl_dim_in] = NULL;

	return dim;
}

__isl_give isl_space *isl_space_flatten(__isl_take isl_space *dim)
{
	if (!dim)
		return NULL;
	if (!dim->nested[0] && !dim->nested[1])
		return dim;

	if (dim->nested[0])
		dim = isl_space_reset(dim, isl_dim_in);
	if (dim && dim->nested[1])
		dim = isl_space_reset(dim, isl_dim_out);

	return dim;
}

__isl_give isl_space *isl_space_flatten_domain(__isl_take isl_space *dim)
{
	if (!dim)
		return NULL;
	if (!dim->nested[0])
		return dim;

	return isl_space_reset(dim, isl_dim_in);
}

__isl_give isl_space *isl_space_flatten_range(__isl_take isl_space *dim)
{
	if (!dim)
		return NULL;
	if (!dim->nested[1])
		return dim;

	return isl_space_reset(dim, isl_dim_out);
}

/* Replace the dimensions of the given type of dst by those of src.
 */
__isl_give isl_space *isl_space_replace(__isl_take isl_space *dst,
	enum isl_dim_type type, __isl_keep isl_space *src)
{
	dst = isl_space_cow(dst);

	if (!dst || !src)
		goto error;

	dst = isl_space_drop_dims(dst, type, 0, isl_space_dim(dst, type));
	dst = isl_space_add_dims(dst, type, isl_space_dim(src, type));
	dst = copy_ids(dst, type, 0, src, type);

	if (dst && type == isl_dim_param) {
		int i;
		for (i = 0; i <= 1; ++i) {
			if (!dst->nested[i])
				continue;
			dst->nested[i] = isl_space_replace(dst->nested[i],
							 type, src);
			if (!dst->nested[i])
				goto error;
		}
	}

	return dst;
error:
	isl_space_free(dst);
	return NULL;
}

/* Given a dimension specification "dim" of a set, create a dimension
 * specification for the lift of the set.  In particular, the result
 * is of the form [dim -> local[..]], with n_local variables in the
 * range of the wrapped map.
 */
__isl_give isl_space *isl_space_lift(__isl_take isl_space *dim, unsigned n_local)
{
	isl_space *local_dim;

	if (!dim)
		return NULL;

	local_dim = isl_space_dup(dim);
	local_dim = isl_space_drop_dims(local_dim, isl_dim_set, 0, dim->n_out);
	local_dim = isl_space_add_dims(local_dim, isl_dim_set, n_local);
	local_dim = isl_space_set_tuple_name(local_dim, isl_dim_set, "local");
	dim = isl_space_join(isl_space_from_domain(dim),
			    isl_space_from_range(local_dim));
	dim = isl_space_wrap(dim);
	dim = isl_space_set_tuple_name(dim, isl_dim_set, "lifted");

	return dim;
}

int isl_space_can_zip(__isl_keep isl_space *dim)
{
	if (!dim)
		return -1;

	return dim->nested[0] && dim->nested[1];
}

__isl_give isl_space *isl_space_zip(__isl_take isl_space *dim)
{
	isl_space *dom, *ran;
	isl_space *dom_dom, *dom_ran, *ran_dom, *ran_ran;

	if (!isl_space_can_zip(dim))
		isl_die(dim->ctx, isl_error_invalid, "dim cannot be zipped",
			goto error);

	if (!dim)
		return 0;
	dom = isl_space_unwrap(isl_space_domain(isl_space_copy(dim)));
	ran = isl_space_unwrap(isl_space_range(dim));
	dom_dom = isl_space_domain(isl_space_copy(dom));
	dom_ran = isl_space_range(dom);
	ran_dom = isl_space_domain(isl_space_copy(ran));
	ran_ran = isl_space_range(ran);
	dom = isl_space_join(isl_space_from_domain(dom_dom),
			   isl_space_from_range(ran_dom));
	ran = isl_space_join(isl_space_from_domain(dom_ran),
			   isl_space_from_range(ran_ran));
	return isl_space_join(isl_space_from_domain(isl_space_wrap(dom)),
			    isl_space_from_range(isl_space_wrap(ran)));
error:
	isl_space_free(dim);
	return NULL;
}

int isl_space_has_named_params(__isl_keep isl_space *dim)
{
	int i;
	unsigned off;

	if (!dim)
		return -1;
	if (dim->nparam == 0)
		return 1;
	off = isl_space_offset(dim, isl_dim_param);
	if (off + dim->nparam > dim->n_id)
		return 0;
	for (i = 0; i < dim->nparam; ++i)
		if (!dim->ids[off + i])
			return 0;
	return 1;
}

/* Align the initial parameters of dim1 to match the order in dim2.
 */
__isl_give isl_space *isl_space_align_params(__isl_take isl_space *dim1,
	__isl_take isl_space *dim2)
{
	isl_reordering *exp;

	if (!isl_space_has_named_params(dim1) || !isl_space_has_named_params(dim2))
		isl_die(isl_space_get_ctx(dim1), isl_error_invalid,
			"parameter alignment requires named parameters",
			goto error);

	dim2 = isl_space_params(dim2);
	exp = isl_parameter_alignment_reordering(dim1, dim2);
	exp = isl_reordering_extend_space(exp, dim1);
	isl_space_free(dim2);
	if (!exp)
		return NULL;
	dim1 = isl_space_copy(exp->dim);
	isl_reordering_free(exp);
	return dim1;
error:
	isl_space_free(dim1);
	isl_space_free(dim2);
	return NULL;
}

/* Given the space of set (domain), construct a space for a map
 * with as domain the given space and as range the range of "model".
 */
__isl_give isl_space *isl_space_extend_domain_with_range(
	__isl_take isl_space *domain, __isl_take isl_space *model)
{
	isl_space *space;

	space = isl_space_from_domain(domain);
	space = isl_space_add_dims(space, isl_dim_out,
				    isl_space_dim(model, isl_dim_out));
	if (isl_space_has_tuple_id(model, isl_dim_out))
		space = isl_space_set_tuple_id(space, isl_dim_out,
				isl_space_get_tuple_id(model, isl_dim_out));
	isl_space_free(model);
	return space;
}
