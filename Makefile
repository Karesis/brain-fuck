.PHONY: clean build all

all: build build/bf build/bfopt

build: 
	@mkdir -p build 

build/bf: src/bf.c build 
	clang -Wall -Wextra -O3 -std=c23 $< -o $@

build/bfopt: src/bfopt.c build 
	clang -Wall -Wextra -O3 -std=c23 $< -o $@

clean:
	rm -rf build