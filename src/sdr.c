#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <assert.h>
#include "GLES3/gl32.h"

#if defined(unix) || defined(__unix__)
#include <unistd.h>
#include <sys/stat.h>
#endif	/* unix */

#include "sdr.h"

static const char *sdrtypestr(unsigned int sdrtype);
//static int sdrtypeidx(unsigned int sdrtype);

unsigned int create_vertex_shader(const char *src)
{
	return create_shader(src, GL_VERTEX_SHADER);
}

unsigned int create_pixel_shader(const char *src)
{
	return create_shader(src, GL_FRAGMENT_SHADER);
}

unsigned int create_shader(const char *src, unsigned int sdr_type)
{
	unsigned int sdr;
	int success, info_len;
	char *info_str = 0;
	const char *src_str[3];
	int src_str_count = 0;
	GLenum err;

	sdr = glCreateShader(sdr_type);
	assert(glGetError() == GL_NO_ERROR);
	glShaderSource(sdr, src_str_count, src_str, 0);
	err = glGetError();
	assert(err == GL_NO_ERROR);
	glCompileShader(sdr);
	assert(glGetError() == GL_NO_ERROR);

	glGetShaderiv(sdr, GL_COMPILE_STATUS, &success);
	assert(glGetError() == GL_NO_ERROR);
	glGetShaderiv(sdr, GL_INFO_LOG_LENGTH, &info_len);
	assert(glGetError() == GL_NO_ERROR);

	if(info_len) {
		if((info_str = (char *)malloc(info_len + 1))) {
			glGetShaderInfoLog(sdr, info_len, 0, info_str);
			assert(glGetError() == GL_NO_ERROR);
			info_str[info_len] = 0;
		}
	}

	if(success) {
		fprintf(stderr, info_str ? "done: %s\n" : "done\n", info_str);
	} else {
		fprintf(stderr, info_str ? "failed: %s\n" : "failed\n", info_str);
		glDeleteShader(sdr);
		sdr = 0;
	}

	free(info_str);
	return sdr;
}

void free_shader(unsigned int sdr)
{
	glDeleteShader(sdr);
}

unsigned int load_vertex_shader(const char *fname)
{
	return load_shader(fname, GL_VERTEX_SHADER);
}

unsigned int load_pixel_shader(const char *fname)
{
	return load_shader(fname, GL_FRAGMENT_SHADER);
}

unsigned int load_shader(const char *fname, unsigned int sdr_type)
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

	if(!(src = (char*)malloc(filesize + 1))) {
		fclose(fp);
		return 0;
	}
	fread(src, 1, filesize, fp);
	src[filesize] = 0;
	fclose(fp);

	fprintf(stderr, "compiling %s shader: %s... ", sdrtypestr(sdr_type), fname);
	sdr = create_shader(src, sdr_type);

	free(src);
	return sdr;
}

/* ---- gpu programs ---- */

unsigned int create_program(void)
{
	unsigned int prog = glCreateProgram();
	assert(glGetError() == GL_NO_ERROR);
	return prog;
}

unsigned int create_program_link(unsigned int sdr0, ...)
{
	unsigned int prog, sdr;
	va_list ap;

	if(!(prog = create_program())) {
		return 0;
	}

	attach_shader(prog, sdr0);
	if(glGetError()) {
		return 0;
	}

	va_start(ap, sdr0);
	while((sdr = va_arg(ap, unsigned int))) {
		attach_shader(prog, sdr);
		if(glGetError()) {
			return 0;
		}
	}
	va_end(ap);

	if(link_program(prog) == -1) {
		free_program(prog);
		return 0;
	}
	return prog;
}

unsigned int create_program_load(const char *vfile, const char *pfile)
{
	unsigned int vs = 0, ps = 0;

	if(vfile && *vfile && !(vs = load_vertex_shader(vfile))) {
		return 0;
	}
	if(pfile && *pfile && !(ps = load_pixel_shader(pfile))) {
		return 0;
	}
	return create_program_link(vs, ps, 0);
}

void free_program(unsigned int sdr)
{
	glDeleteProgram(sdr);
}

void attach_shader(unsigned int prog, unsigned int sdr)
{
	int err;

	if(prog && sdr) {
		assert(glGetError() == GL_NO_ERROR);
		glAttachShader(prog, sdr);
		if((err = glGetError()) != GL_NO_ERROR) {
			fprintf(stderr, "failed to attach shader %u to program %u (err: 0x%x)\n", sdr, prog, err);
			abort();
		}
	}
}

int link_program(unsigned int prog)
{
	int linked, info_len, retval = 0;
	char *info_str = 0;

	glLinkProgram(prog);
	assert(glGetError() == GL_NO_ERROR);
	glGetProgramiv(prog, GL_LINK_STATUS, &linked);
	assert(glGetError() == GL_NO_ERROR);
	glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &info_len);
	assert(glGetError() == GL_NO_ERROR);

	if(info_len) {
		if((info_str = (char*)malloc(info_len + 1))) {
			glGetProgramInfoLog(prog, info_len, 0, info_str);
			assert(glGetError() == GL_NO_ERROR);
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

int bind_program(unsigned int prog)
{
	GLenum err;

	glUseProgram(prog);
	if(prog && (err = glGetError()) != GL_NO_ERROR) {
		/* maybe the program is not linked, try linking first */
		if(err == GL_INVALID_OPERATION) {
			if(link_program(prog) == -1) {
				return -1;
			}
			glUseProgram(prog);
			return glGetError() == GL_NO_ERROR ? 0 : -1;
		}
		return -1;
	}
	return 0;
}

static const char *sdrtypestr(unsigned int sdrtype)
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

#if 0
static int sdrtypeidx(unsigned int sdrtype)
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
#endif
