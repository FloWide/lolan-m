CFLAGS=

SRCS=$(wildcard *.c)

OBJS=$(SRCS:.c=.o)

%.o : %.c
	gcc -c $(CFLAGS) $< -o $@

all : $(OBJS)
	ar rcs liblolan.a $(OBJS)

clean:
	rm -f *.o
	rm -f *.a
