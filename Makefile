NOECHO=@

c_files=commons gui main
build_objects=$(patsubst %, build/%.o, $(c_files))
src_files=$(patsubst %, src/%.c, $(c_files))

help:
	$(NOECHO)echo "Type \"make build\" if you want to"

build/%.o: src/%.c build_dir
	gcc -std=c99 -g -Wall -pedantic -D_POSIX_C_SOURCE=199309L -D_XOPEN_SOURCE=500 -c $< -o $@

bin/snake: bin_dir
	gcc $(build_objects) -o $@

build: $(build_objects) bin/snake
	$(NOECHO)echo Done!

clean:
	rm -rf ./build/
	rm -rf ./bin/

%_dir:
	$(NOECHO)mkdir -p $*

.PHONY: build clean
