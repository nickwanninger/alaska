
.PHONY: FORCE

all: paper.pdf

paper.pdf: FORCE
	./latexrun paper.tex

clean:
	./latexrun --clean-all

check-fonts: 
	pdffonts paper.pdf

proof:
	@echo "weasel words: "
	@echo 
	@sh weasel *.tex
	@echo
	@echo "passive voice: "
	@echo 
	@sh passive *.tex
	@echo
	@echo "duplicates (e.g. on separate lines): "
	@echo
	perl dups *.tex



wasp:
	make -C src

install:
	make install -C src


.PHONY: clean check-fonts proof
