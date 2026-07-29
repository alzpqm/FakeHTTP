CROSS_PREFIX :=
CC=$(CROSS_PREFIX)gcc
STRIP=$(CROSS_PREFIX)strip

PREFIX=/usr/local
BINDIR=$(PREFIX)/bin
SRCDIR=src
INCLUDEDIR=include
BUILDDIR=build
SRCS := $(wildcard $(SRCDIR)/*.c)
OBJS := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SRCS))

override CFLAGS+=-std=c99 -I$(INCLUDEDIR) -frandom-seed=fakehttp \
	-pedantic -Wall -Wextra -Wdate-time
override LDFLAGS+=-lnetfilter_queue -lnfnetlink -lmnl

ifdef VERSION
	override CFLAGS += -DVERSION=\"$(VERSION)\"
endif

FAKEHTTP=$(BUILDDIR)/fakehttp
TEST_CORE=$(BUILDDIR)/test-core-validation
TEST_PACKET=$(BUILDDIR)/test-packet-validation
TEST_SIGNALS=$(BUILDDIR)/test-signals

ifeq ($(STATIC), 1)
	override LDFLAGS += -static
endif

ifeq ($(DEBUG), 1)
	override CFLAGS += -O0 -g3 -fsanitize=address,leak,undefined
	override LDFLAGS += -fsanitize=address,leak,undefined
else
	override CFLAGS += -O3
endif

all: $(FAKEHTTP)

debug:
	$(MAKE) DEBUG=1

test: all $(TEST_CORE) $(TEST_PACKET) $(TEST_SIGNALS)
	scripts/test-cli-validation.sh $(FAKEHTTP)
	$(TEST_CORE)
	$(TEST_PACKET)
	scripts/test-signal-validation.sh $(TEST_SIGNALS)

clean:
	$(RM) -r $(BUILDDIR)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/%.d: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -MM -MT $(@:.d=.o) $< -MF $@

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(FAKEHTTP): $(OBJS) $(MKS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)
ifneq ($(DEBUG), 1)
	$(STRIP) $@
endif

$(TEST_CORE): tests/test-core-validation.c $(BUILDDIR)/globvar.o \
	$(BUILDDIR)/logging.o $(BUILDDIR)/payload.o $(BUILDDIR)/srcinfo.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(TEST_PACKET): tests/test-packet-validation.c $(BUILDDIR)/globvar.o \
	$(BUILDDIR)/logging.o $(BUILDDIR)/ipv4pkt.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(TEST_SIGNALS): tests/test-signals.c $(BUILDDIR)/globvar.o \
	$(BUILDDIR)/logging.o $(BUILDDIR)/signals.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	install -m 755 $(FAKEHTTP) $(DESTDIR)$(BINDIR)/fakehttp

uninstall:
	$(RM) $(DESTDIR)$(BINDIR)/fakehttp

.PHONY: all debug test clean install uninstall

ifneq ($(MAKECMDGOALS),clean)
-include $(OBJS:.o=.d)
endif
