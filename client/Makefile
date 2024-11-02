CFLAGS = -I.
MAKEFLAGS += --silent
CC=gcc -g

INCLUDE = include
OUTPUT = build
SOURCE = src


all: 
	mkdir -p $(OUTPUT)
	$(CC) $(CFLAGS) -o $(OUTPUT)/client $(SOURCE)/*.c

clean: 
	rm -rf $(OUTPUT)/*
	mkdir -p $(OUTPUT)

