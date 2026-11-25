CC      = gcc
CFLAGS  = -Wall -Wextra -g -Iinclude
SRC     = src/main.c src/server.c src/api.c src/orders.c src/queue.c src/menu.c src/storage.c src/strbuf.c
TARGET  = kitchen

ifeq ($(OS),Windows_NT)
  LIBS = -lws2_32
endif

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LIBS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe kitchen.dat
