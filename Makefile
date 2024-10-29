TARGET    = libcnear.a
OBJS      = src/cnear.o src/base64.o src/cJSON.o \
	ed25519-donna/curve25519-donna-32bit.o ed25519-donna/curve25519-donna-helpers.o ed25519-donna/curve25519-donna-scalarmult-base.o ed25519-donna/ed25519-donna-32bit-tables.o ed25519-donna/ed25519-donna-basepoint-table.o ed25519-donna/ed25519-donna-impl-base.o ed25519-donna/ed25519-keccak.o ed25519-donna/ed25519-sha3.o ed25519-donna/ed25519.o ed25519-donna/modm-donna-32bit.o \
	ed25519-donna/memzero.o ed25519-donna/sha2.o ed25519-donna/sha3.o \


PREFIX   = 
CC       = $(PREFIX)gcc
CXX      = $(PREFIX)g++
AR       = $(PREFIX)ar
CFLAGS   = -Wall -D_GNU_SOURCE -Iinclude -Ied25519-donna -I/opt/homebrew/include
CXXFLAGS = $(CFLAGS) -std=gnu++11

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(AR) rcu $@ $^

clean:
	@rm -rf $(TARGET) $(OBJS)
	@echo "Cleaned up!"

tests: $(TARGET)
	$(CC) $(CFLAGS) -o cnear-test tests/main.c $(TARGET) -lcurl

install: $(TARGET)
	@echo Copying...
	@cp -frv include/cnear.h $(SDKPATH)/ports/include
	@cp -frv $(TARGET) $(SDKPATH)/ports/lib/$(TARGET)
	@echo lib installed!
