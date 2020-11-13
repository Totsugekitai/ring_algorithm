MAKE			= make
LATEXMK			= latexmk

LATEXMKFLAG	   += -halt-on-error -lualatex

CP				= cp
RM				= rm

SRC				= main

TARGET			= $(addsuffix .pdf $(SRC))

.PHONY: all continue clean

all: $(SRC).pdf

%.pdf: %.tex
	$(LATEXMK) $(LATEXMKFLAG) $<

continue: LATEXMKFLAG += -pvc -synctex=1
continue: all

clean:
	rm -f *.bbl *.blg *.aux *.log *.dvi *.fls *.fdb_latexmk *.ltjruby *.out *.synctex.gz *.pdf

