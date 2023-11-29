

subsystems = \
	core cpu device hal util

targets =

ifneq ("$(wildcard config.rules)","")
include config.rules
else
$(error config.rules not found, run configure first)
endif

cflags  += -std=c2x -D_GNU_SOURCE -MMD -MP
ldflags +=

build    = build
librvx86 = $(build)/librvx86.a

srcs = $(foreach system,$(subsystems),$(sort $(wildcard source/$(system)/*.c)))
objs = $(patsubst source/%.c,$(build)/%.o,$(filter %.c,$(srcs)))
prgs = $(targets:%=$(build)/%.o)
deps = $(objs:%.o=%.d) $(prgs:%.o=%.d)


.PHONY: all build clean purge $(targets)
.PHONY: .FORCE
.FORCE:


all: $(targets)


build:
	mkdir -p $(sort $(dir $(objs)))

clean:
	rm -rf $(build)

purge: clean
	rm -f $(config.rules)


-include $(deps)


$(targets): %: build $(build)/%

$(targets:%=$(build)/%): $(build)/%: $(patsubst %,$(build)/%.o,%) $(librvx86)
	$(ld) $^ $(ldflags) $(libs:%=-L-l%) -o $@

$(librvx86): $(objs)
	$(ar) rcs $@ $^

$(objs) $(prgs): $(build)/%.o: source/%.c
	$(cc) -Isource/ $(cflags) -c $< -o $@

