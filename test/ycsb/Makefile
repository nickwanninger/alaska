CXX       = gclang++
CC        = gclang
CXXFLAGS  = -std=c++17 -O2 -Wall
CFLAGS    = -std=c11 -O2 -Wall
TARGET    = bin/ycsb

all: $(TARGET) bin/ycsb-alaska

bin/:
	mkdir -p bin

OBJS = bin/ycsb.o bin/core_workload.o bin/dict.o bin/sds.o bin/mt19937-64.o bin/siphash.o

$(TARGET): $(OBJS) | bin/
	$(CXX) $(CXXFLAGS) -o $@ $^ -ldl

bin/ycsb.bc: $(TARGET)
	get-bc -b -o $@ $(TARGET)
	opt -O3 $@ -o $@
	opt -strip-debug $@ -o $@

bin/ycsb-alaska.bc: bin/ycsb.bc
	alaska-transform $< -o $@

bin/ycsb-full.o: bin/ycsb-alaska.bc
	llc -O3 $< --relocation-model=pic --filetype=obj -o $@

bin/ycsb-alaska: bin/ycsb-full.o | bin/
	clang++ $< $(shell alaska-config --ldflags --cflags) -lm -ldl -o $@

bin/ycsb.o: ycsb.cpp | bin/
	$(CXX) $(CXXFLAGS) -c -o $@ $<

bin/core_workload.o: core_workload.cc | bin/
	$(CXX) $(CXXFLAGS) -c -o $@ $<

bin/dict.o: dict.c | bin/
	$(CC) $(CFLAGS) -c -o $@ $<

bin/sds.o: sds.c | bin/
	$(CC) $(CFLAGS) -c -o $@ $<

bin/mt19937-64.o: mt19937-64.c | bin/
	$(CC) $(CFLAGS) -c -o $@ $<

bin/siphash.o: siphash.c | bin/
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	$(RM) -r bin/
