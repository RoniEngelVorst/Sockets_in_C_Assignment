CC = gcc
CFLAGS = -Wall -Wextra -g
SRC_RECEIVER = TCP_Receiver.c
SRC_SENDER = TCP_Sender.c
OBJ_RECEIVER = $(SRC_RECEIVER:.c=.o)
OBJ_SENDER = $(SRC_SENDER:.c=.o)
DEPS = $(SRC_RECEIVER:.c=.d) $(SRC_SENDER:.c=.d)



all: TCP_Receiver TCP_Sender

TCP_Receiver: $(OBJ_RECEIVER)
	$(CC) $(CFLAGS) -o $@ $^

TCP_Sender: $(OBJ_SENDER)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

-include $(DEPS)

.PHONY: all clean

clean:
	rm -f *.o *.d TCP_Receiver TCP_Sender
