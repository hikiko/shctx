#ifndef SDR_H
#define SDR_H

/* ---- shaders ---- */
unsigned int create_vertex_shader(const char *src);
unsigned int create_pixel_shader(const char *src);
unsigned int create_shader(const char *src, unsigned int sdr_type);
void free_shader(unsigned int sdr);

unsigned int load_vertex_shader(const char *fname);
unsigned int load_pixel_shader(const char *fname);
unsigned int load_shader(const char *src, unsigned int sdr_type);

int add_shader(const char *fname, unsigned int sdr);
int remove_shader(const char *fname);

/* ---- gpu programs ---- */
unsigned int create_program(void);
unsigned int create_program_link(unsigned int sdr0, ...);
unsigned int create_program_load(const char *vfile, const char *pfile);
void free_program(unsigned int sdr);

void attach_shader(unsigned int prog, unsigned int sdr);
int link_program(unsigned int prog);
int bind_program(unsigned int prog);

#endif //SDR_H
