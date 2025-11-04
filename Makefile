VERSION=1.2

CC = gcc
CFLAGS = -g -Wall $(shell pkg-config --cflags gtk+-3.0 libserialport) -Iinclude
LDFLAGS = $(shell pkg-config --libs gtk+-3.0 libserialport)
GLADE_FILE = rfid_ui.glade
GLADE_HEADER = src/rfidUiGlade.c
SRC = $(wildcard src/*.c)
OBJDIR = build
OBJ = $(patsubst src/%.c, $(OBJDIR)/%.o, $(SRC))
BIN = dist/refid_attack_demo

all: $(BIN)

$(GLADE_HEADER): $(GLADE_FILE)
	xxd -i $< > $@

$(OBJDIR)/gui.o: src/gui.c $(GLADE_HEADER) include/gui.h
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: src/%.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN): $(OBJ)
	@mkdir -p dist
	$(CC) $(OBJ) -o $(BIN) $(LDFLAGS)

clean:
	rm -rf $(OBJDIR) $(BIN)

DEB_DIR = install

deb: all
	cp $(BIN) $(DEB_DIR)/usr/local/bin/
	dpkg-deb --build $(DEB_DIR) dist/rfid_attack_demo_$(VERSION).deb

uninstall-deb:
	sudo dpkg -r rfid-attack-demo