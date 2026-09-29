CXX ?= c++
CC ?= cc
BISON ?= bison
FLEX ?= flex
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -Isrc -Ibuild
SOURCES = src/main.cpp src/compiler.cpp src/backend.cpp build/parser.cpp build/lexer.cpp

.PHONY: all clean test examples
all: build/sclpc build/runtime.o

build:
	mkdir -p build

build/parser.cpp: src/parser.y src/ast.hpp | build
	$(BISON) -d -o $@ src/parser.y

build/parser.hpp: build/parser.cpp
	@test -f $@

build/lexer.cpp: src/lexer.l build/parser.hpp
	$(FLEX) -o $@ $<

build/sclpc: $(SOURCES) src/ast.hpp src/compiler.hpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SOURCES) -o $@

build/runtime.o: src/runtime.c | build
	$(CC) -std=c11 -O2 -Wall -Wextra -c $< -o $@

build/%.s: examples/%.sclp build/sclpc
	build/sclpc -O1 $< -o $@

build/%: build/%.s build/runtime.o
	$(CC) $< build/runtime.o -o $@

examples: build/factorial build/control_flow build/recursion build/arithmetic

test: all
	python3 tests/run_tests.py

clean:
	rm -rf build
