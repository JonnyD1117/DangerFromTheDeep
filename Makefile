.PHONY: install, config, build, run, clean

clean:
	rm -rf ./build/*
	rm -rf ./bin/*

config:
	cmake -S . -B ./build

build: config
	cmake --build ./build

install:
