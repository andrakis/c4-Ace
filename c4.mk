# c4.mk -- this game's part of c4's Makefile. c4 includes it from
# games/ace/c4.mk, its submodule, and runs it from c4's root, with c4's
# tools and variables (games/README.md in c4). Without it, c4 builds
# everything else, and test-ace says it was skipped.

# The Ace of Penguins (games/ace/README.c4.md): lib/'s units with c4lc,
# c4/xwin-c4.c in the place of lib/xwin.c, each game's own, and what
# upstream generates at build time: the images and the help, here by
# Python's standard library (c4/imglib.py, c4/text2c.py). Those are generated
# into .ace/gen, not committed. The same objects for C4IX, the bare machine
# and C4DOS but for the platform's (.ace/plat). taipedit stays out: it draws
# with X itself.
ACE        := games/ace
ACE_GAMES  := solitaire canfield freecell golf mastermind merlin minesweeper pegged spider taipei thornq
ACE_LIB    := table help stack imagelib funcs table_rn
ACE_HDRS   := $(wildcard $(ACE)/lib/*.h $(ACE)/games/*.h $(ACE)/c4/*.h $(ACE)/c4/X11/*.h) \
              $(wildcard src/c4ix/libc/include/*.h src/c4ix/libc/include/sys/*.h) \
              src/c4ix/include/display.h src/c4ix/include/window.h src/c4ix/include/window.c \
              libjs/guest/gui.h libjs/guest/gui.c
# rand and srand: lib/table_rn.c's, so a seed deals the same on both
# compilers, and not libc's
ACE_DEFS   := -D C4ACE -D rand=ace_rand -D srand=ace_srand
ACE_CC     := ./c4sp32 -c 400000 src/c4sp/lisp/c4lc.lisp -O $(IMG_G) -mfuse -fno-builtin -c -conforming \
              $(ACE_DEFS) -I $(ACE)/c4 -I $(ACE)/lib -I $(ACE)/games -I src/c4ix/libc/include \
              -I src/c4ix/include -I libjs/guest

# each game's images, as upstream's games/Makefile.am lists them
ACE_IMG_solitaire   := solitaire
ACE_IMG_canfield    := canfield
ACE_IMG_freecell    := freecell
ACE_IMG_golf        := golf golf-arrow golf-noarrow
ACE_IMG_mastermind  := mastermind mastermind-b mastermind-c mastermind-e mastermind-eb mastermind-g \
                       mastermind-k mastermind-o mastermind-p mastermind-r mastermind-w mastermind-y
ACE_IMG_merlin      := merlin merlin-c merlin-b
ACE_IMG_minesweeper := minesweeper minesweeper-x minesweeper-t minesweeper-c12 minesweeper-c24 \
                       minesweeper-c36 minesweeper-c48
ACE_IMG_pegged      := pegged pegged-h pegged-p
ACE_IMG_spider      := spider golf-arrow golf-noarrow
ACE_IMG_taipei      := taipei taipei-tiles
ACE_IMG_thornq      := thornq thornq-arrow thornq-noarrow
# what a game has besides its own unit
ACE_MORE_taipei     := taipeilib

ACE_GEN    := .ace/gen
# a game's objects, without the platform's
ace-objs    = $(1:%=.ace/g/%.c4o) $(1:%=.ace/g/%-img.c4o) $(1:%=.ace/g/%-help.c4o) $(1:%=.ace/g/%-demo.c4o) \
              $(ACE_MORE_$(1):%=.ace/g/%.c4o)
ACE_OBJS   := $(foreach g,$(ACE_GAMES),$(call ace-objs,$(g)))
ACE_LOBJS  := $(ACE_LIB:%=.ace/lib/%.c4o) .ace/lib/xwin-c4.c4o .ace/lib/cards-img.c4o
ace-objs: $(ACE_OBJS) $(ACE_LOBJS) .ace/plat/plat-c4ix.c4o .ace/plat/plat-bare.c4o
PHONY += ace-objs

# what is generated, and the objects of it: this is what becomes of lib/png
# and games/*.png and *.html
$(ACE_GEN)/cards-img.c: $(ACE)/c4/imglib.py $(ACE)/lib/images.in $(wildcard $(ACE)/lib/png/*.png)
	@mkdir -p $(ACE_GEN)
	python3 -I $(ACE)/c4/imglib.py -n cards -i $(ACE)/lib/png $@ @$(ACE)/lib/images.in
define ACE_GAME_GEN
$(ACE_GEN)/$(1)-img.c: $(ACE)/c4/imglib.py $(ACE_IMG_$(1):%=$(ACE)/games/%.png)
	@mkdir -p $(ACE_GEN)
	python3 -I $(ACE)/c4/imglib.py -n appimglib -i $(ACE)/games $$@ $(ACE_IMG_$(1))
$(ACE_GEN)/$(1)-help.c: $(ACE)/c4/text2c.py $(wildcard $(ACE)/games/$(1).html)
	@mkdir -p $(ACE_GEN)
	$(if $(wildcard $(ACE)/games/$(1).html),python3 -I $(ACE)/c4/text2c.py $(1)_help $(ACE)/games/$(1).html $$@,echo 'char $(1)_help[] = "";' > $$@)
$(ACE_GEN)/$(1)-demo.c: $(ACE)/c4/text2c.py $(ACE)/c4/scripts/$(1).txt
	@mkdir -p $(ACE_GEN)
	python3 -I $(ACE)/c4/text2c.py ace_demo_script $(ACE)/c4/scripts/$(1).txt $$@
endef
$(foreach g,$(ACE_GAMES),$(eval $(call ACE_GAME_GEN,$(g))))

.ace/lib/%.c4o: $(ACE)/lib/%.c $(ACE_HDRS) $(C4LC_LISP) .img_g | c4sp32
	@mkdir -p .ace/lib
	$(ACE_CC) $< $@ > /dev/null
.ace/lib/xwin-c4.c4o: $(ACE)/c4/xwin-c4.c $(ACE)/c4/font.h $(ACE_HDRS) $(C4LC_LISP) .img_g | c4sp32
	@mkdir -p .ace/lib
	$(ACE_CC) $< $@ > /dev/null
.ace/lib/cards-img.c4o: $(ACE_GEN)/cards-img.c $(ACE_HDRS) $(C4LC_LISP) .img_g | c4sp32
	@mkdir -p .ace/lib
	$(ACE_CC) $< $@ > /dev/null
.ace/g/%.c4o: $(ACE)/games/%.c $(ACE_HDRS) $(C4LC_LISP) .img_g | c4sp32
	@mkdir -p .ace/g
	$(ACE_CC) $< $@ > /dev/null
.ace/g/%-img.c4o: $(ACE_GEN)/%-img.c $(ACE_HDRS) $(C4LC_LISP) .img_g | c4sp32
	@mkdir -p .ace/g
	$(ACE_CC) $< $@ > /dev/null
.ace/g/%-help.c4o: $(ACE_GEN)/%-help.c $(C4LC_LISP) .img_g | c4sp32
	@mkdir -p .ace/g
	$(ACE_CC) $< $@ > /dev/null
.ace/g/%-demo.c4o: $(ACE_GEN)/%-demo.c $(C4LC_LISP) .img_g | c4sp32
	@mkdir -p .ace/g
	$(ACE_CC) $< $@ > /dev/null
.ace/plat/%.c4o: $(ACE)/c4/%.c $(ACE_HDRS) $(C4LC_LISP) .img_g | c4sp32
	@mkdir -p .ace/plat
	$(ACE_CC) $< $@ > /dev/null

# ... and by gcc for the host, with no display (plat-host.c), the reference
# test-ace holds the c4lc build to; 32-bit, as that is. -fno-strict-aliasing:
# demo_report reads the framebuffer's bytes as words.
ACE_HOST_LIB := $(ACE_LIB:%=$(ACE)/lib/%.c) $(ACE)/c4/xwin-c4.c $(ACE)/c4/plat-host.c
define ACE_GAME_HOST
.ace/host/$(1): $(ACE_HOST_LIB) $(ACE)/games/$(1).c $(ACE_MORE_$(1):%=$(ACE)/games/%.c) $(ACE_GEN)/cards-img.c \
	 $(ACE_GEN)/$(1)-img.c $(ACE_GEN)/$(1)-help.c $(ACE_GEN)/$(1)-demo.c $(ACE_HDRS)
	@mkdir -p .ace/host
	gcc -m32 -O2 -std=gnu99 -w -Wno-error=implicit-function-declaration -Wno-error=int-conversion -fno-strict-aliasing -DC4ACE -Drand=ace_rand -Dsrand=ace_srand \
	  -I$(ACE)/c4 -I$(ACE)/lib -I$(ACE)/games -Isrc/c4ix/include -Ilibjs/guest -o $$@ \
	  $(ACE_HOST_LIB) $(ACE)/games/$(1).c $(ACE_MORE_$(1):%=$(ACE)/games/%.c) $(ACE_GEN)/cards-img.c \
	  $(ACE_GEN)/$(1)-img.c $(ACE_GEN)/$(1)-help.c $(ACE_GEN)/$(1)-demo.c -lm
endef
$(foreach g,$(ACE_GAMES),$(eval $(call ACE_GAME_HOST,$(g))))
ace-host: $(ACE_GAMES:%=.ace/host/%)
PHONY += ace-host

# test-ace: each game's demo (c4/scripts/NAME.txt, -demo), one script on a
# fixed deal and a clock of its own, built by gcc and by c4lc -- on the bare
# machine, under C4DOS and inside C4IX -- must put the same pixels on the
# screen: after every event, a line with the number of frames shown so far,
# the colours in use, a hash of every frame's changed pixels in turn and one
# of the whole screen. The C4DOS and C4IX runs are each one boot, playing
# every game in turn, so each game must also end and give the machine back.
ACE_SEED := 1
ACE_ARGS := -demo -seed $(ACE_SEED)
test-ace: ace-host c4bb-images
	@rm -rf .ace_t && mkdir -p .ace_t && \
	 for g in $(ACE_GAMES); do .ace/host/$$g $(ACE_ARGS) | grep '^ace ' >> .ace_t/host.txt \
	   || { echo "test-ace: gcc's $$g did not run"; exit 1; }; done && \
	 tail -1 .ace_t/host.txt > /dev/null && \
	 for g in $(ACE_GAMES); do \
	   ( cd src/c4bb/images/games && timeout 1200 node ../../../../libjs/cli.js -m 64 -d . $$g.c4r $(ACE_ARGS) 2>/dev/null ) \
	     | grep '^ace ' >> .ace_t/bare.out; done && \
	 { for g in $(ACE_GAMES); do echo "$$g $(ACE_ARGS)"; done; echo ver; } | \
	   ( cd src/c4bb/images/games && timeout 2400 node ../../../../libjs/cli.js -m 64 -d . ../c4dos32.c4r 2>/dev/null ) > .ace_t/dos.out; \
	 { for g in $(ACE_GAMES); do echo "$$g $(ACE_ARGS)"; done; echo exit; } | \
	   timeout 2400 node libjs/cli.js -m 64 -d src/c4bb/images/disk src/c4bb/images/c4ix32.c4r 2>/dev/null > .ace_t/c4ix.out; \
	 for w in bare dos c4ix; do sed -e 's/^A:.>//' .ace_t/$$w.out | grep '^ace ' > .ace_t/$$w.txt; \
	   cmp -s .ace_t/host.txt .ace_t/$$w.txt \
	   || { echo "test-ace: $$w's screens differ from gcc's:"; diff .ace_t/host.txt .ace_t/$$w.txt | head -5; \
	        tail -5 .ace_t/$$w.out; exit 1; }; done && \
	 { grep -Eq 'C4DOS version' .ace_t/dos.out || { echo "test-ace: C4DOS did not come back to its prompt:"; \
	     tail -5 .ace_t/dos.out; exit 1; }; } && \
	 echo "test-ace: $(words $(ACE_GAMES)) games, $$(grep -c '^ace [0-9]' .ace_t/host.txt) events: OK" && rm -rf .ace_t
PHONY += test-ace

GAME_OBJS  += ace-objs
GAME_TESTS += test-ace
