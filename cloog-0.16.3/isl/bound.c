#include <assert.h>
#include <isl/stream.h>
#include <isl_polynomial_private.h>
#include <isl_scan.h>

struct bound_options {
	struct isl_options	*isl;
	unsigned		 verify;
	int			 print_all;
	int			 continue_on_error;
};

struct isl_arg bound_options_arg[] = {
ISL_ARG_CHILD(struct bound_options, isl, "isl", isl_options_arg, "isl options")
ISL_ARG_BOOL(struct bound_options, verify, 'T', "verify", 0, NULL)
ISL_ARG_BOOL(struct bound_options, print_all, 'A', "print-all", 0, NULL)
ISL_ARG_BOOL(struct bound_options, continue_on_error, '\0', "continue-on-error", 0, NULL)
ISL_ARG_END
};

ISL_ARG_DEF(bound_options, struct bound_options, bound_options_arg)

static __isl_give isl_set *set_bounds(__isl_take isl_set *set)
{
	unsigned nparam;
	int i, r;
	isl_point *pt, *pt2;
	isl_set *box;

	nparam = isl_set_dim(set, isl_dim_param);
	r = nparam >= 8 ? 5 : nparam >= 5 ? 15 : 50;

	pt = isl_set_sample_point(isl_set_copy(set));
	pt2 = isl_point_copy(pt);

	for (i = 0; i < nparam; ++i) {
		pt = isl_point_add_ui(pt, isl_dim_param, i, r);
		pt2 = isl_point_sub_ui(pt2, isl_dim_param, i, r);
	}

	box = isl_set_box_from_points(pt, pt2);

	return isl_set_intersect(set, box);
}

struct verify_point_bound {
	struct bound_options *options;
	int stride;
	int n;
	int exact;
	int error;

	isl_pw_qpolynomial_fold *pwf;
	isl_pw_qpolynomial_fold *bound;
};

static int verify_point(__isl_take isl_point *pnt, void *user)
{
	int i;
	unsigned nvar;
	unsigned nparam;
	struct verify_point_bound *vpb = (struct verify_point_bound *) user;
	isl_int t;
	isl_pw_qpolynomial_fold *pwf;
	isl_qpolynomial *bound = NULL;
	isl_qpolynomial *opt = NULL;
	isl_set *dom = NULL;
	const char *minmax;
	int bounded;
	int sign;
	int ok;
	FILE *out = vpb->options->print_all ? stdout : stderr;

	vpb->n--;

	if (1) {
		minmax = "ub";
		sign = 1;
	} else {
		minmax = "lb";
		sign = -1;
	}

	isl_int_init(t);

	pwf = isl_pw_qpolynomial_fold_copy(vpb->pwf);

	nparam = isl_pw_qpolynomial_fold_dim(pwf, isl_dim_param);
	for (i = 0; i < nparam; ++i) {
		isl_point_get_coordinate(pnt, isl_dim_param, i, &t);
		pwf = isl_pw_qpolynomial_fold_fix_dim(pwf, isl_dim_param, i, t);
	}

	bound = isl_pw_qpolynomial_fold_eval(
				    isl_pw_qpolynomial_fold_copy(vpb->bound),
				    isl_point_copy(pnt));

	dom = isl_pw_qpolynomial_fold_domain(isl_pw_qpolynomial_fold_copy(pwf));
	bounded = isl_set_is_bounded(dom);

	if (bounded < 0)
		goto error;

	if (!bounded)
		opt = isl_pw_qpolynomial_fold_eval(
				    isl_pw_qpolynomial_fold_copy(pwf),
				    isl_set_sample_point(isl_set_copy(dom)));
	else if (sign > 0)
		opt = isl_pw_qpolynomial_fold_max(isl_pw_qpolynomial_fold_copy(pwf));
	else
		opt = isl_pw_qpolynomial_fold_min(isl_pw_qpolynomial_fold_copy(pwf));

	nvar = isl_set_dim(dom, isl_dim_set);
	opt = isl_qpolynomial_drop_dims(opt, isl_dim_set, 0, nvar);
	if (vpb->exact && bounded)
		ok = isl_qpolynomial_plain_is_equal(opt, bound);
	else if (sign > 0)
		ok = isl_qpolynomial_le_cst(opt, bound);
	else
		ok = isl_qpolynomial_le_cst(bound, opt);
	if (ok < 0)
		goto error;

	if (vpb->options->print_all || !ok) {
		fprintf(out, "%s(", minmax);
		for (i = 0; i < nparam; ++i) {
			if (i)
				fprintf(out, ", ");
			isl_point_get_coordinate(pnt, isl_dim_param, i, &t);
			isl_int_print(out, t, 0);
		}
		fprintf(out, ") = ");
		isl_qpolynomial_print(bound, out, ISL_FORMAT_ISL);
		fprintf(out, ", %s = ", bounded ? "opt" : "sample");
		isl_qpolynomial_print(opt, out, ISL_FORMAT_ISL);
		if (ok)
			fprintf(out, ". OK\n");
		else
			fprintf(out, ". NOT OK\n");
	} else if ((vpb->n % vpb->stride) == 0) {
		printf("o");
		fflush(stdout);
	}

	if (0) {
error:
		ok = 0;
	}

	isl_pw_qpolynomial_fold_free(pwf);
	isl_qpolynomial_free(bound);
	isl_qpolynomial_free(opt);
	isl_point_free(pnt);
	isl_set_free(dom);

	isl_int_clear(t);

	if (!ok)
		vpb->error = 1;

	if (vpb->options->continue_on_error)
		ok = 1;

	return (vpb->n >= 1 && ok) ? 0 : -1;
}

static int check_solution(__isl_take isl_pw_qpolynomial_fold *pwf,
	__isl_take isl_pw_qpolynomial_fold *bound, int exact,
	struct bound_options *options)
{
	struct verify_point_bound vpb;
	isl_int count, max;
	isl_set *dom;
	isl_set *context;
	int i, r, n;

	dom = isl_pw_qpolynomial_fold_domain(isl_pw_qpolynomial_fold_copy(pwf));
	context = isl_set_remove_dims(isl_set_copy(dom), isl_dim_set,
					0, isl_set_dim(dom, isl_dim_set));
	context = isl_set_remove_divs(context);
	context = set_bounds(context);

	isl_int_init(count);
	isl_int_init(max);

	isl_int_set_si(max, 200);
	r = isl_set_count_upto(context, max, &count);
	assert(r >= 0);
	n = isl_int_get_si(count);

	isl_int_clear(max);
	isl_int_clear(count);

	vpb.options = options;
	vpb.pwf = pwf;
	vpb.bound = bound;
	vpb.n = n;
	vpb.stride = n > 70 ? 1 + (n + 1)/70 : 1;
	vpb.error = 0;
	vpb.exact = exact;

	if (!options->print_all) {
		for (i = 0; i < vpb.n; i += vpb.stride)
			printf(".");
		printf("\r");
		fflush(stdout);
	}

	isl_set_foreach_point(context, verify_point, &vpb);

	isl_set_free(context);
	isl_set_free(dom);
	isl_pw_qpolynomial_fold_free(pwf);
	isl_pw_qpolynomial_fold_free(bound);

	if (!options->print_all)
		printf("\n");

	if (vpb.error) {
		fprintf(stderr, "Check failed !\n");
		return -1;
	}

	return 0;
}

int main(int argc, char **argv)
{
	isl_ctx *ctx;
	isl_pw_qpolynomial_fold *copy;
	isl_pw_qpolynomial_fold *pwf;
	struct isl_stream *s;
	struct isl_obj obj;
	struct bound_options *options;
	int exact;
	int r = 0;

	options = bound_options_new_with_defaults();
	assert(options);
	argc = bound_options_parse(options, argc, argv, ISL_ARG_ALL);

	ctx = isl_ctx_alloc_with_options(bound_options_arg, options);

	s = isl_stream_new_file(ctx, stdin);
	obj = isl_stream_read_obj(s);
	if (obj.type == isl_obj_pw_qpolynomial)
		pwf = isl_pw_qpolynomial_fold_from_pw_qpolynomial(isl_fold_max,
								  obj.v);
	else if (obj.type == isl_obj_pw_qpolynomial_fold)
		pwf = obj.v;
	else {
		obj.type->free(obj.v);
		isl_die(ctx, isl_error_invalid, "invalid input", goto error);
	}

	if (options->verify)
		copy = isl_pw_qpolynomial_fold_copy(pwf);

	pwf = isl_pw_qpolynomial_fold_bound(pwf, &exact);
	pwf = isl_pw_qpolynomial_fold_coalesce(pwf);

	if (options->verify) {
		r = check_solution(copy, pwf, exact, options);
	} else {
		if (!exact)
			printf("# NOT exact\n");
		isl_pw_qpolynomial_fold_print(pwf, stdout, 0);
		fprintf(stdout, "\n");
		isl_pw_qpolynomial_fold_free(pwf);
	}

error:
	isl_stream_free(s);

	isl_ctx_free(ctx);

	return r;
}
