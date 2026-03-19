CXX       = gclang++
CC        = gclang
CXXFLAGS  = -std=c++17 -O2 -Wall
CFLAGS    = -std=c11 -O2 -Wall
TARGET    = ycsb

all: $(TARGET)

$(TARGET): ycsb.o core_workload.o dict.o sds.o mt19937-64.o siphash.o
	$(CXX) $(CXXFLAGS) -o $@ $^ -ldl
	get-bc -b -o $(TARGET).bc $(TARGET)
	opt -O3 $(TARGET).bc -o $(TARGET).bc
	opt -strip-debug $(TARGET).bc -o $(TARGET).bc


	alaska-transform $(TARGET).bc -o $(TARGET)-alaska.bc
	llc -O3 $(TARGET)-alaska.bc --relocation-model=pic --filetype=obj -o ycsb-full.o
	clang++ ycsb-full.o $(shell alaska-config --ldflags --cflags) -lm -ldl -o $(TARGET)-alaska






ycsb.o: ycsb.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

core_workload.o: core_workload.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $<

dict.o: dict.c
	$(CC) $(CFLAGS) -c -o $@ $<

sds.o: sds.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	$(RM) $(TARGET) *.o

mt19937-64.o: mt19937-64.c
	$(CC) $(CFLAGS) -c -o $@ $<

siphash.o: siphash.c
	$(CC) $(CFLAGS) -c -o $@ $<
