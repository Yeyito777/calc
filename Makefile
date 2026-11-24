CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lm

TARGET = calc

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c $(LDFLAGS)

install: $(TARGET)
	mkdir -p $(HOME)/.local/bin
	cp $(TARGET) $(HOME)/.local/bin

clean:
	rm -f $(TARGET) a.out

debug:
	$(CC) -DDEBUG $(CFLAGS) -o $(TARGET) main.c $(LDFLAGS)
