# CC = gcc
# CFLAGS = -Wall -Wextra -g
# SRC_RECEIVER = TCP_Receiver.c
# SRC_SENDER = TCP_Sender.c
# OBJ_RECEIVER = $(SRC_RECEIVER:.c=.o)
# OBJ_SENDER = $(SRC_SENDER:.c=.o)
# DEPS = $(SRC_RECEIVER:.c=.d) $(SRC_SENDER:.c=.d)



# all: TCP_Receiver TCP_Sender

# TCP_Receiver: $(OBJ_RECEIVER)
# 	$(CC) $(CFLAGS) -o $@ $^

# TCP_Sender: $(OBJ_SENDER)
# 	$(CC) $(CFLAGS) -o $@ $^

# %.o: %.c
# 	$(CC) $(CFLAGS) -c $< -o $@

# -include $(DEPS)

# .PHONY: all clean

# clean:
# 	rm -f *.o *.d TCP_Receiver TCP_Sender

CC = gcc
CFLAGS = -Wall -Wextra -std=c99
LDFLAGS =

# Source files
SENDER_SRC = RUDP_Sender.c RUDP_API.c
RECEIVER_SRC = RUDP_Receiver.c RUDP_API.c

# Object files
SENDER_OBJ = $(SENDER_SRC:.c=.o)
RECEIVER_OBJ = $(RECEIVER_SRC:.c=.o)

# Executables
SENDER_EXEC = RUDP_Sender
RECEIVER_EXEC = RUDP_Receiver

.PHONY: all clean

all: $(SENDER_EXEC) $(RECEIVER_EXEC)

$(SENDER_EXEC): $(SENDER_OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

$(RECEIVER_EXEC): $(RECEIVER_OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	$(RM) $(SENDER_OBJ) $(RECEIVER_OBJ) $(SENDER_EXEC) $(RECEIVER_EXEC)

