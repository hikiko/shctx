csrc = $(wildcard src/*.c)
src = $(wildcard src/*.cc)
obj = $(src:.cc=.o) $(csrc:.c=.o)
dep = $(src:.cc=.d) $(csrc:.c=.d)
bin = shctx

libpath = /home/eleni/igalia/code/angle/out/Debug
lib = -L$(libpath) -Wl,-rpath=$(libpath)
inc = -I/home/eleni/igalia/install/include

CXXFLAGS = -pedantic -Wall -g $(inc) -MMD
LIBS = $(lib) -lGLESv2 -lEGL
LDFLAGS = -lX11 $(LIBS)

$(bin): $(obj) Makefile
	$(CXX) -o $@ $(obj) $(LDFLAGS)

-include $(dep)

.PHONY: clean
clean:
	rm -f $(obj) $(bin)

.PHONY: cleandep
cleandep:
	rm -f $(dep)
