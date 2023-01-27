INPUT=test #test, train, ref
VERSION=rate #speed, rate
BENCHMARK=all #mcf_r, lbm_s, xz_r, ...

all: baseline_code_generation

optimized_code_generation: compile_all bitcode bitcode_copy setup optimization binary run

baseline_code_generation: compile_all bitcode bitcode_copy setup binary

SPEC2017:
	tar -xf $(TAR)
	chmod +x -R $@/bin $@/tools $@/*.sh
	./scripts/install.sh

compile_speed: SPEC2017
	./scripts/compile.sh pure_c_cpp_speed

compile_rate: SPEC2017
	./scripts/compile.sh pure_c_cpp_rate

compile_all: SPEC2017
	./scripts/compile.sh pure_c_cpp_rate;
	./scripts/compile.sh pure_c_cpp_speed;

bitcode: SPEC2017
	./scripts/bitcode.sh rate
	./scripts/bitcode.sh speed

bitcode_copy: SPEC2017
	./scripts/bitcode_copy.sh

setup: SPEC2017
	./scripts/setup.sh test rate;
	./scripts/setup.sh train rate;
	./scripts/setup.sh ref rate;
	./scripts/setup.sh test speed;
	./scripts/setup.sh train speed;
	./scripts/setup.sh ref speed;

optimization: SPEC2017
	./scripts/optimization.sh $(BENCHMARK)

binary: SPEC2017
	./scripts/$@.sh $(BENCHMARK)
	
run: SPEC2017
	./scripts/$@.sh $(INPUT) $(BENCHMARK)

clean:
	rm -f *.txt 
	cd condor ; make clean ;
	rm -f tmp* ;

uninstall: clean
	./scripts/uninstall.sh

.PHONY: compile_speed compile_rate compile_all bitcode setup optimization binary run clean
