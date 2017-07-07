# 指令编译及选项
CC = gcc
CFLAGS=-Wall -std=gnu99
DEF = -DDEBUG
CFLAGS += $(DEF)
#-Wall:提示所有waring、gnu99:对应 C99 的 GNU 扩展
TARGET = main
SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)
$(TARGET):$(OBJS)
	$(CC) $(CFLAGS) -o $@ $^
clean:
	rm -f *.o main
