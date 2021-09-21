SHELL = /bin/sh
CC    = g++
FLAGS        = -std=c++11 -Iinclude
CFLAGS       = -fPIC -Wall -Wextra -march=native -ggdb3
LDFLAGS      = -shared 
DEBUGFLAGS   = -O0 -D _DEBUG
RELEASEFLAGS = -O3 -D NDEBUG 


TARGET     = libsplitudr.so
OUTPUT_DIR = Linux_x64
SOURCES    = $(shell echo *.cpp)
HEADERS    = $(shell echo include/*.h)
OBJECTS    = $(SOURCES:.c=.o)

PREFIX = $(DESTDIR)/usr/local
BINDIR = $(PREFIX)/bin

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(FLAGS) $(CFLAGS) $(LDFLAGS) $(RELEASEFLAGS) -o $(OUTPUT_DIR)/$(TARGET) $(OBJECTS) $(STATIC_LIBS)
