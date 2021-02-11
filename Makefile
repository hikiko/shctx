csrc = $(wildcard src/*.c)
src = $(wildcard src/*.cc)
obj = $(src:.cc=.o) $(csrc:.c=.o)
dep = $(src:.cc=.d) $(csrc:.c=.d)
bin = shctx

lib = -L/home/eleni/igalia/install/lib
inc = -I/home/eleni/igalia/install/include

CXXFLAGS = -pedantic -Wall -g $(inc) -MMD
LDFLAGS = $(lib) -lGLESv2 -lEGL -lX11

$(bin): $(obj)
	$(CXX) -o $@ $(obj) $(LDFLAGS)

-include $(dep)

.PHONY: clean
clean:
	rm -f $(obj) $(bin)

.PHONY: cleandep
cleandep:
	rm -f $(dep)
