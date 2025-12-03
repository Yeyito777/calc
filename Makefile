CC = gcc
CFLAGS = -Wall -O2
LDFLAGS = -lm

TARGET = calc 

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c $(LDFLAGS)

install: $(TARGET)
	mkdir -p /home/yeyito/.local/bin
	cp $(TARGET) /home/yeyito/.local/bin

clean:
	rm -f $(TARGET) a.out

debug:
	$(CC) -DDEBUG $(CFLAGS) -o $(TARGET) main.c $(LDFLAGS)
