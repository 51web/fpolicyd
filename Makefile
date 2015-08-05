CC=		gcc
SRCS=		$(wildcard *.c)
OBJS=		$(SRCS:.c = .o)
BIN=		fpolicyd
CFLAGS+=	-Wall -Werror -I/usr/include/libev -fno-builtin-strlen
LDFLAGS+=	-lev

default:	$(OBJS)
		$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $(BIN)

%.o:		%.c
		$(CC) -c $< $(CFLAGS)

install:
clean:
		rm -f *.o
		rm -f $(BIN)
