LD = $(CC)

SRCDIR = src
OUTDIR = build
BINNAME = cnes

SRCEXT = c
OBJEXT = o

INCLUDES = ./include
LIBS = c

INCFLAGS = $(foreach d, $(INCLUDES), -I$d)
LIBFLAGS = $(foreach d, $(LIBS), -l$d)
CFLAGS = $(INCFLAGS) -Wall -pedantic-errors -std=c11
LDFLAGS = $(LIBFLAGS)

SRCFILES = $(wildcard $(SRCDIR)/*.$(SRCEXT))

OBJFILES = $(patsubst %, $(OUTDIR)/%, $(patsubst $(SRCDIR)/%.$(SRCEXT),   %.$(OBJEXT), $(SRCFILES)))

MKDIR_P = @mkdir -p

all: gendirs apply_license_header compile

apply_license_header:
	./scripts/apply_license_headers.sh

compile: $(OBJFILES)
	$(LD) $(LDFLAGS) -o $(OUTDIR)/$(BINNAME) $(OBJFILES)

.SECONDEXPANSION:
$(OUTDIR)/%.$(OBJEXT): $$(wildcard $(SRCDIR)/%.$(SRCEXT))
	$(CC) $(CFLAGS) -MD -c -o $@ $<
	@cp $(OUTDIR)/$*.d $(OUTDIR)/$*.P; \
			sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
			-e '/^$$/ d' -e 's/$$/ :/' < $(OUTDIR)/$*.d >> $(OUTDIR)/$*.P; \
			rm -f $(OUTDIR)/$*.d

-include *.P

clean:
	rm -rf $(OUTDIR)

.PHONY: gendirs

gendirs: ${OUTDIR}

${OUTDIR}:
	${MKDIR_P} ${OUTDIR}
