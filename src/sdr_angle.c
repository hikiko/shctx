/*
 * GLSL shader management, part of:
 * https://github.com/jtsiomb/dropcode
 * Author: John Tsiombikas <nuclear@member.fsf.org>
 * This code is placed in the public domain
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <assert.h>

#include "mygl.h"

#if defined(unix) || defined(__unix__)
#include <unistd.h>
#include <sys/stat.h>
#endif	/* unix */

#include "sdr_angle.h"

static const char *sdrtypestr_angle(unsigned int sdrtype);
static int sdrtypeidx_angle(unsigned int sdrtype);


unsigned int create_vertex_shader_angle(const char *src)
{
	return create_shader_angle(src, GL_VERTEX_SHADER);
}

unsigned int create_pixel_shader_angle(const char *src)
{
	return create_shader_angle(src, GL_FRAGMENT_SHADER);
}

unsigned int create_tessctl_shader_angle(const char *src)
{
#ifdef GL_TESS_CONTROL_SHADER
	return create_shader_angle(src, GL_TESS_CONTROL_SHADER);
#else
	return 0;
#endif
}

unsigned int create_tesseval_shader_angle(const char *src)
{
#ifdef GL_TESS_EVALUATION_SHADER
	return create_shader_angle(src, GL_TESS_EVALUATION_SHADER);
#else
	return 0;
#endif
}

unsigned int create_geometry_shader_angle(const char *src)
{
#ifdef GL_GEOMETRY_SHADER
	return create_shader(src, GL_GEOMETRY_SHADER);
#else
	return 0;
#endif
}

unsigned int create_shader_angle(const char *src, unsigned int sdr_type)
{
	unsigned int sdr;
	int success, info_len;
	char *info_str = 0;
	const char *src_str[3], *header, *footer;
	int src_str_count = 0;
	GLenum err;

	if((header = get_shader_header_angle(sdr_type))) {
		src_str[src_str_count++] = header;
	}
	src_str[src_str_count++] = src;
	if((footer = get_shader_footer_angle(sdr_type))) {
		src_str[src_str_count++] = footer;
	}

	sdr = angle_glCreateShader(sdr_type);
	assert(angle_glGetError() == GL_NO_ERROR);
	angle_glShaderSource(sdr, src_str_count, src_str, 0);
	err = angle_glGetError();
	assert(err == GL_NO_ERROR);
	angle_glCompileShader(sdr);
	assert(angle_glGetError() == GL_NO_ERROR);

	angle_glGetShaderiv(sdr, GL_COMPILE_STATUS, &success);
	assert(angle_glGetError() == GL_NO_ERROR);
	angle_glGetShaderiv(sdr, GL_INFO_LOG_LENGTH, &info_len);
	assert(angle_glGetError() == GL_NO_ERROR);

	if(info_len) {
		if((info_str = malloc(info_len + 1))) {
			angle_glGetShaderInfoLog(sdr, info_len, 0, info_str);
			assert(angle_glGetError() == GL_NO_ERROR);
			info_str[info_len] = 0;
		}
	}

	if(success) {
		fprintf(stderr, info_str ? "done: %s\n" : "done\n", info_str);
	} else {
		fprintf(stderr, info_str ? "failed: %s\n" : "failed\n", info_str);
		angle_glDeleteShader(sdr);
		sdr = 0;
	}

	free(info_str);
	return sdr;
}

void free_shader_angle(unsigned int sdr)
{
	angle_glDeleteShader(sdr);
}

unsigned int load_vertex_shader_angle(const char *fname)
{
	return load_shader_angle(fname, GL_VERTEX_SHADER);
}

unsigned int load_pixel_shader_angle(const char *fname)
{
	return load_shader_angle(fname, GL_FRAGMENT_SHADER);
}

unsigned int load_tessctl_shader_angle(const char *fname)
{
#ifdef GL_TESS_CONTROL_SHADER
	return load_shader_angle(fname, GL_TESS_CONTROL_SHADER);
#else
	return 0;
#endif
}

unsigned int load_tesseval_shader_angle(const char *fname)
{
#ifdef GL_TESS_EVALUATION_SHADER
	return load_shader_angle(fname, GL_TESS_EVALUATION_SHADER);
#else
	return 0;
#endif
}

unsigned int load_geometry_shader_angle(const char *fname)
{
#ifdef GL_GEOMETRY_SHADER
	return load_shader_angle(fname, GL_GEOMETRY_SHADER);
#else
	return 0;
#endif
}

unsigned int load_shader_angle(const char *fname, unsigned int sdr_type)
{
	unsigned int sdr;
	size_t filesize;
	FILE *fp;
	char *src;

	if(!(fp = fopen(fname, "rb"))) {
		fprintf(stderr, "failed to open shader %s: %s\n", fname, strerror(errno));
		return 0;
	}

	fseek(fp, 0, SEEK_END);
	filesize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if(!(src = malloc(filesize + 1))) {
		fclose(fp);
		return 0;
	}
	fread(src, 1, filesize, fp);
	src[filesize] = 0;
	fclose(fp);

	fprintf(stderr, "compiling %s shader: %s... ", sdrtypestr_angle(sdr_type), fname);
	sdr = create_shader_angle(src, sdr_type);

	free(src);
	return sdr;
}


/* ---- gpu programs ---- */

unsigned int create_program_angle(void)
{
	unsigned int prog = angle_glCreateProgram();
	assert(angle_glGetError() == GL_NO_ERROR);
	return prog;
}

unsigned int create_program_link_angle(unsigned int sdr0, ...)
{
	unsigned int prog, sdr;
	va_list ap;

	if(!(prog = create_program_angle())) {
		return 0;
	}

	attach_shader_angle(prog, sdr0);
	if(angle_glGetError()) {
		return 0;
	}

	va_start(ap, sdr0);
	while((sdr = va_arg(ap, unsigned int))) {
		attach_shader_angle(prog, sdr);
		if(angle_glGetError()) {
			return 0;
		}
	}
	va_end(ap);

	if(link_program_angle(prog) == -1) {
		free_program_angle(prog);
		return 0;
	}
	return prog;
}

unsigned int create_program_load_angle(const char *vfile, const char *pfile)
{
	unsigned int vs = 0, ps = 0;

	if(vfile && *vfile && !(vs = load_vertex_shader_angle(vfile))) {
		return 0;
	}
	if(pfile && *pfile && !(ps = load_pixel_shader_angle(pfile))) {
		return 0;
	}
	return create_program_link_angle(vs, ps, 0);
}

void free_program_angle(unsigned int sdr)
{
	angle_glDeleteProgram(sdr);
}

void attach_shader_angle(unsigned int prog, unsigned int sdr)
{
	int err;

	if(prog && sdr) {
		assert(angle_glGetError() == GL_NO_ERROR);
		angle_glAttachShader(prog, sdr);
		if((err = angle_glGetError()) != GL_NO_ERROR) {
			fprintf(stderr, "failed to attach shader %u to program %u (err: 0x%x)\n", sdr, prog, err);
			abort();
		}
	}
}

int link_program_angle(unsigned int prog)
{
	int linked, info_len, retval = 0;
	char *info_str = 0;

	angle_glLinkProgram(prog);
	assert(angle_glGetError() == GL_NO_ERROR);
	angle_glGetProgramiv(prog, GL_LINK_STATUS, &linked);
	assert(angle_glGetError() == GL_NO_ERROR);
	angle_glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &info_len);
	assert(angle_glGetError() == GL_NO_ERROR);

	if(info_len) {
		if((info_str = malloc(info_len + 1))) {
			angle_glGetProgramInfoLog(prog, info_len, 0, info_str);
			assert(angle_glGetError() == GL_NO_ERROR);
			info_str[info_len] = 0;
		}
	}

	if(linked) {
		fprintf(stderr, info_str ? "linking done: %s\n" : "linking done\n", info_str);
	} else {
		fprintf(stderr, info_str ? "linking failed: %s\n" : "linking failed\n", info_str);
		retval = -1;
	}

	free(info_str);
	return retval;
}

int bind_program_angle(unsigned int prog)
{
	GLenum err;

	angle_glUseProgram(prog);
	if(prog && (err = angle_glGetError()) != GL_NO_ERROR) {
		/* maybe the program is not linked, try linking first */
		if(err == GL_INVALID_OPERATION) {
			if(link_program_angle(prog) == -1) {
				return -1;
			}
			angle_glUseProgram(prog);
			return angle_glGetError() == GL_NO_ERROR ? 0 : -1;
		}
		return -1;
	}
	return 0;
}

/* ugly but I'm not going to write the same bloody code over and over */
#define BEGIN_UNIFORM_CODE \
	int loc, curr_prog; \
	angle_glGetIntegerv(GL_CURRENT_PROGRAM, &curr_prog); \
	if((unsigned int)curr_prog != prog && bind_program_angle(prog) == -1) { \
		return -1; \
	} \
	if((loc = angle_glGetUniformLocation(prog, name)) != -1)

#define END_UNIFORM_CODE \
	if((unsigned int)curr_prog != prog) { \
		bind_program_angle(curr_prog); \
	} \
	return loc == -1 ? -1 : 0

int get_uniform_loc_angle(unsigned int prog, const char *name)
{
	int loc, curr_prog;
	angle_glGetIntegerv(GL_CURRENT_PROGRAM, &curr_prog);
	if((unsigned int)curr_prog != prog && bind_program_angle(prog) == -1) {
		return -1;
	}
	loc = angle_glGetUniformLocation(prog, name);
	if((unsigned int)curr_prog != prog) {
		bind_program_angle(curr_prog);
	}
	return loc;
}

int set_uniform_int_angle(unsigned int prog, const char *name, int val)
{
	BEGIN_UNIFORM_CODE {
		angle_glUniform1i(loc, val);
	}
	END_UNIFORM_CODE;
}

int get_attrib_loc_angle(unsigned int prog, const char *name)
{
	int loc, curr_prog;

	angle_glGetIntegerv(GL_CURRENT_PROGRAM, &curr_prog);
	if((unsigned int)curr_prog != prog && bind_program_angle(prog) == -1) {
		return -1;
	}

	loc = angle_glGetAttribLocation(prog, (char*)name);

	if((unsigned int)curr_prog != prog) {
		bind_program_angle(curr_prog);
	}
	return loc;
}

/* ---- shader composition ---- */
struct string {
	char *text;
	int len;
};

#define NUM_SHADER_TYPES	5
static struct string header[NUM_SHADER_TYPES];
static struct string footer[NUM_SHADER_TYPES];

static void clear_string_angle(struct string *str)
{
	free(str->text);
	str->text = 0;
	str->len = 0;
}

static void append_string_angle(struct string *str, const char *s)
{
	int len, newlen;
	char *newstr;

	if(!s || !*s) return;

	len = strlen(s);
	newlen = str->len + len;
	if(!(newstr = malloc(newlen + 2))) {	/* leave space for a possible newline */
		fprintf(stderr, "shader composition: failed to append string of size %d\n", len);
		abort();
	}

	if(str->text) {
		memcpy(newstr, str->text, str->len);
	}
	memcpy(newstr + str->len, s, len + 1);

	if(s[len - 1] != '\n') {
		newstr[newlen] = '\n';
		newstr[newlen + 1] = 0;
	}

	free(str->text);
	str->text = newstr;
	str->len = newlen;
}

void clear_shader_header_angle(unsigned int type)
{
	if(type) {
		int idx = sdrtypeidx_angle(type);
		clear_string_angle(&header[idx]);
	} else {
		int i;
		for(i=0; i<NUM_SHADER_TYPES; i++) {
			clear_string_angle(&header[i]);
		}
	}
}

void clear_shader_footer_angle(unsigned int type)
{
	if(type) {
		int idx = sdrtypeidx_angle(type);
		clear_string_angle(&footer[idx]);
	} else {
		int i;
		for(i=0; i<NUM_SHADER_TYPES; i++) {
			clear_string_angle(&footer[i]);
		}
	}
}

void add_shader_header_angle(unsigned int type, const char *s)
{
	if(type) {
		int idx = sdrtypeidx_angle(type);
		append_string_angle(&header[idx], s);
	} else {
		int i;
		for(i=0; i<NUM_SHADER_TYPES; i++) {
			append_string_angle(&header[i], s);
		}
	}
}

void add_shader_footer_angle(unsigned int type, const char *s)
{
	if(type) {
		int idx = sdrtypeidx_angle(type);
		append_string_angle(&footer[idx], s);
	} else {
		int i;
		for(i=0; i<NUM_SHADER_TYPES; i++) {
			append_string_angle(&footer[i], s);
		}
	}
}

const char *get_shader_header_angle(unsigned int type)
{
	int idx = sdrtypeidx_angle(type);
	return header[idx].text;
}

const char *get_shader_footer_angle(unsigned int type)
{
	int idx = sdrtypeidx_angle(type);
	return footer[idx].text;
}

static const char *sdrtypestr_angle(unsigned int sdrtype)
{
	switch(sdrtype) {
	case GL_VERTEX_SHADER:
		return "vertex";
	case GL_FRAGMENT_SHADER:
		return "pixel";
	default:
		break;
	}
	return "<unknown>";
}

static int sdrtypeidx_angle(unsigned int sdrtype)
{
	switch(sdrtype) {
	case GL_VERTEX_SHADER:
		return 0;
	case GL_FRAGMENT_SHADER:
		return 1;
	default:
		break;
	}
	return 0;
}
