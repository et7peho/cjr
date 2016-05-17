CC=gcc
DEPS =
OBJ = cjr.o
EXECUTABLE = cjr
CFLAGS = -g  -Wall -I. #-DPETER_DEBUG
CFLAGS += `pkg-config --cflags libcurl`
CFLAGS +=`xml2-config --cflags`
LDFLAGS += `pkg-config --libs libcurl`
LDFLAGS += `xml2-config --libs`
GIT_VERSION := $(shell git describe --abbrev=7 --dirty --always --tags)

CFLAGS += -DVERSION=\"$(GIT_VERSION)\"
all: $(EXECUTABLE)

cjr.o: cjr.c
	$(CC) -c cjr.c -o cjr.o $(CFLAGS)

#%.o: %.c $(DEPS)
#	$(CC) -c  $@ $< $(CFLAGS)

$(EXECUTABLE): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

clean:
	rm -f $(OBJ) $(EXECUTABLE)
