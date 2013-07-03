CC = gcc
CFLAGS = 
LIBS = 
SRC_FILES = *.c

all: tproxy_example

tproxy_example: $(SRC_FILES)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -f tproxy_example *.o

.PHONY: clean
