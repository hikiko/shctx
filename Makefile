csrc = $(wildcard src/*.c)
src = $(wildcard src/*.cc)
obj = $(src:.cc=.o) $(csrc:.c=.o)
dep = $(src:.cc=.d) $(csrc:.c=.d)
bin = shctx

libpath = /home/eleni/igalia/code/angle/out/Debug
lib = -L/home/eleni/igalia/install/lib
inc = -I/home/eleni/igalia/install/include

CXXFLAGS = -pedantic -Wall -g $(inc) -MMD -DANGLEPATH=\"$(libpath)\" -DANGLE
LDFLAGS = $(lib) -lGLESv2 -lEGL -lX11 -ldl

$(bin): $(obj) Makefile
	$(CXX) -o $@ $(obj) $(LDFLAGS)

-include $(dep)

.PHONY: clean
clean:
	rm -f $(obj) $(bin)

.PHONY: cleandep
cleandep:
	rm -f $(dep)
