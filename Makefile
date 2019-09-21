rwildcard=$(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2) $(filter $(subst *,%,$2),$d))

LD = $(CC)

SRCDIR = src
INCDIR = include

OUTDIR = build
BINNAME = cnes

SRCEXT = c
HEXT   = h
OBJEXT = o

INCLUDES = ./include
LIBS = c c6502 pthread SDL2
CFLAGS := -g -Wall -std=c11

INCFLAGS := $(foreach d, $(INCLUDES), -I$d)
LIBFLAGS := $(foreach d, $(LIBS), -l$d)
CFLAGS += $(INCFLAGS)
LDFLAGS := $(LIBFLAGS)

SRCFILES = $(call rwildcard, $(SRCDIR), *.$(SRCEXT))
HFILES = $(call rwildcard, $(INCDIR), *.$(HEXT))

OBJFILES = $(patsubst %, $(OUTDIR)/%, $(patsubst $(SRCDIR)/%.$(SRCEXT), %.$(OBJEXT), $(SRCFILES)))

MKDIR_P = @mkdir -p

.PHONY: apply_license_headers clean all

all: apply_license_headers $(OUTDIR)/$(BINNAME)

apply_license_headers:
	./scripts/apply_license_headers.sh

$(OUTDIR)/$(BINNAME): $(OBJFILES)
	$(LD) -o $(OUTDIR)/$(BINNAME) $(OBJFILES) $(LDFLAGS)

.SECONDEXPANSION:
$(OUTDIR)/%.$(OBJEXT): $(SRCDIR)/%.$(SRCEXT)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MD -c -o $@ $<
	@cp $(OUTDIR)/$*.d $(OUTDIR)/$*.P; \
			sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
			-e '/^$$/ d' -e 's/$$/ :/' < $(OUTDIR)/$*.d >> $(OUTDIR)/$*.P; \
			rm -f $(OUTDIR)/$*.d

-include *.P

clean:
	rm -rf $(OUTDIR)

${OUTDIR}:
	${MKDIR_P} ${OUTDIR}
