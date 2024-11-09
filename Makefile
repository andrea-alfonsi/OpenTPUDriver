.PHONY: all
all:
	cd driver && make clean && make
	cd client && make clean && make

