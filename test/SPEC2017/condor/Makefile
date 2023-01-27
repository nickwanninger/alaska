EMAIL=

all: optimize.con submit

submit: 
	./scripts/submit.sh *.con

speedup.con:
	python2 scripts/generateCondorScript.py scripts/$@ ./$@ "$@" " " ${EMAIL};

optimize.con:
	./scripts/generateCondorScript.sh $@ " " ".bc"

binary.con:
	./scripts/generateCondorScript.sh $@ "baseline_parallelized" ".bc"

run.con:
	./scripts/generateCondorScript.sh $@ " " "_newbin"

clean:
	rm -rf log/*.out log/*.err log/*.log *~ ./*.con

.PHONY: all run clean
