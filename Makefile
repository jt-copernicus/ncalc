#Licensed under GPL-3.0-or-later
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17
LIBS = -lncursesw -lginac -lcln

TARGET = ncalc
SRC = main.cpp
MAN = ncalc.1

PREFIX ?= /usr/local

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LIBS)

install: all
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin
	install -d $(DESTDIR)$(PREFIX)/share/man/man1
	install -m 644 $(MAN) $(DESTDIR)$(PREFIX)/share/man/man1

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin$(TARGET)
	rm -f $(DESTDIR)$(PREFIX)/share/man/man1/$(MAN)

clean:
	rm -f $(TARGET)

.PHONY: all clean install uninstall
