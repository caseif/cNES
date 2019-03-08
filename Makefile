rwildcard=$(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2) $(filter $(subst *,%,$2),$d))

LD = $(CC)

TESTDIR = test

SRCDIR = src
INCDIR = include
TEST_SRCDIR = $(TESTDIR)/src
TEST_INCDIR = $(TESTDIR)/include

OUTDIR = build
BINNAME = cnes

TEST_BINNAME = cnes_test

SRCEXT = c
HEXT   = h
OBJEXT = o

INCLUDES = ./include
TEST_INCLUDES = ./test/include
LIBS = c pthread SDL2

INCFLAGS := $(foreach d, $(INCLUDES), -I$d)
TEST_INCFLAGS := $(foreach d, $(TEST_INCLUDES), -I$d)
LIBFLAGS := $(foreach d, $(LIBS), -l$d)
CFLAGS   := -g $(INCFLAGS) -Wall -std=c11
TEST_CFLAGS := $(TEST_INCFLAGS) $(CFLAGS)
LDFLAGS  := $(LIBFLAGS)

SRCFILES = $(call rwildcard, $(SRCDIR), *.$(SRCEXT))
HFILES = $(call rwildcard, $(INCDIR), *.$(HEXT))
TEST_SRCFILES = $(call rwildcard, $(TEST_SRCDIR), *.$(SRCEXT))
TEST_HFILES = $(call rwildcard, $(TEST_INCDIR), *.$(HEXT))

OBJFILES = $(patsubst %, $(OUTDIR)/%, $(patsubst $(SRCDIR)/%.$(SRCEXT), %.$(OBJEXT), $(SRCFILES)))

TEST_OBJFILES := $(patsubst %, $(OUTDIR)/$(TESTDIR)/%, $(patsubst $(TEST_SRCDIR)/%.$(SRCEXT), %.$(OBJEXT), $(TEST_SRCFILES)))
TEST_OBJFILES += $(filter-out build/main.o,$(OBJFILES))

MKDIR_P = @mkdir -p

.PHONY: apply_license_headers clean all test

all: apply_license_headers $(OUTDIR)/$(BINNAME)

test: $(OUTDIR)/$(TEST_BINNAME)
	./build/cnes_test

apply_license_headers:
	./scripts/apply_license_headers.sh

$(OUTDIR)/$(BINNAME): $(OBJFILES)
	$(LD) -o $(OUTDIR)/$(BINNAME) $(OBJFILES) $(LDFLAGS)

$(OUTDIR)/$(TEST_BINNAME): $(TEST_OBJFILES)
	$(LD) -o $(OUTDIR)/$(TEST_BINNAME) $(TEST_OBJFILES) $(LDFLAGS)

.SECONDEXPANSION:
$(OUTDIR)/%.$(OBJEXT): $(SRCDIR)/%.$(SRCEXT)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MD -c -o $@ $<
	@cp $(OUTDIR)/$*.d $(OUTDIR)/$*.P; \
			sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
			-e '/^$$/ d' -e 's/$$/ :/' < $(OUTDIR)/$*.d >> $(OUTDIR)/$*.P; \
			rm -f $(OUTDIR)/$*.d

.SECONDEXPANSION:
$(OUTDIR)/$(TESTDIR)/%.$(OBJEXT): $(TEST_SRCDIR)/%.$(SRCEXT)
	@mkdir -p $(@D)
	$(CC) $(TEST_CFLAGS) -MD -c -o $@ $<
	@cp $(OUTDIR)/$(TESTDIR)/$*.d $(OUTDIR)/$(TESTDIR)/$*.P; \
			sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
			-e '/^$$/ d' -e 's/$$/ :/' < $(OUTDIR)/$(TESTDIR)/$*.d >> $(OUTDIR)/$(TESTDIR)/$*.P; \
			rm -f $(OUTDIR)/$(TESTDIR)/$*.d

-include *.P

clean:
	rm -rf $(OUTDIR)

${OUTDIR}:
	${MKDIR_P} ${OUTDIR}
