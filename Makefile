CC = gcc
CFLAGS = -Wall -Wextra -pedantic -O2 -std=gnu99
LIBS = -lncurses

TARGET = reed
OBJS = reed.o songarr.o mpvproc.o
SRC = src/

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

reed.o: $(SRC)reed.c $(SRC)songarr.h $(SRC)mpvproc.h
	$(CC) $(CFLAGS) -c $(SRC)reed.c

songarr.o: $(SRC)songarr.c $(SRC)songarr.h
	$(CC) $(CFLAGS) -c $(SRC)songarr.c

mpvproc.o: $(SRC)mpvproc.c
	$(CC) $(CFLAGS) -c $(SRC)mpvproc.c

.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET)

