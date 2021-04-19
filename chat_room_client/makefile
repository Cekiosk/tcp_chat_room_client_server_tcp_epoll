SRCS:=$(wildcard *.c)
TARGS:=$(SRCS:%.c=%.out)
CC:=gcc
all:$(TARGS)

%.out:%.c
	$(CC) $< -o $@ -g -Wall
	@#所有文件分开编译
clean:
	rm -rf $(TARGS)
	rm -rf core

