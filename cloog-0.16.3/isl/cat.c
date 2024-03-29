#include <assert.h>
#include <isl/obj.h>
#include <isl/printer.h>
#include <isl/stream.h>

struct isl_arg_choice cat_format[] = {
	{"isl",		ISL_FORMAT_ISL},
	{"omega",	ISL_FORMAT_OMEGA},
	{"polylib",	ISL_FORMAT_POLYLIB},
	{"ext-polylib",	ISL_FORMAT_EXT_POLYLIB},
	{"latex",	ISL_FORMAT_LATEX},
	{0}
};

struct cat_options {
	struct isl_options	*isl;
	unsigned		 format;
};

struct isl_arg cat_options_arg[] = {
ISL_ARG_CHILD(struct cat_options, isl, "isl", isl_options_arg, "isl options")
ISL_ARG_CHOICE(struct cat_options, format, 0, "format", \
	cat_format,	ISL_FORMAT_ISL, "output format")
ISL_ARG_END
};

ISL_ARG_DEF(cat_options, struct cat_options, cat_options_arg)

int main(int argc, char **argv)
{
	struct isl_ctx *ctx;
	struct isl_stream *s;
	struct isl_obj obj;
	struct cat_options *options;
	isl_printer *p;

	options = cat_options_new_with_defaults();
	assert(options);
	argc = cat_options_parse(options, argc, argv, ISL_ARG_ALL);

	ctx = isl_ctx_alloc_with_options(cat_options_arg, options);

	s = isl_stream_new_file(ctx, stdin);
	obj = isl_stream_read_obj(s);
	isl_stream_free(s);

	p = isl_printer_to_file(ctx, stdout);
	p = isl_printer_set_output_format(p, options->format);
	p = obj.type->print(p, obj.v);
	p = isl_printer_end_line(p);
	isl_printer_free(p);

	obj.type->free(obj.v);

	isl_ctx_free(ctx);

	return 0;
}
