src = $(wildcard src/*.cc)
obj = $(src:.cc=.o)
bin = shctx

lib = -L/home/eleni/igalia/install/lib
inc = -I/home/eleni/igalia/install/include

CXXFLAGS = -pedantic -Wall -g $(inc)
LDFLAGS = $(lib) -lGLESv2 -lEGL -lX11

$(bin): $(obj)
	$(CXX) -o $@ $(obj) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(obj) $(bin)
