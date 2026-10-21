# Cross-platform Makefile for sonic-rs-c (Linux, macOS, Windows MinGW/MSYS2)
# For native Windows MSVC (`cl.exe`), use `build.bat`, `build.ps1`, or `CMakeLists.txt`.

CARGO      ?= cargo
CC         ?= cc
CFLAGS     ?= -O2 -Wall -Wextra -std=c11 -Iinclude
RUSTFLAGS  ?= -C target-cpu=native
DIST_DIR   := dist

ifeq ($(OS),Windows_NT)
    EXE_EXT     := .exe
    LDFLAGS_SYS ?= -lws2_32 -luserenv -ladvapi32 -lbcrypt -lntdll
else
    EXE_EXT     :=
    UNAME_S     := $(shell uname -s)
    ifeq ($(UNAME_S),Darwin)
        LDFLAGS_SYS ?= -lpthread -ldl -lm -framework Security -framework CoreFoundation
    else
        LDFLAGS_SYS ?= -lpthread -ldl -lm
    endif
endif

.PHONY: all dist example test-c clean headers

all: dist

dist: src/lib.rs Cargo.toml include/sonic_rs.h
	RUSTFLAGS="$(RUSTFLAGS)" $(CARGO) build --release
	@mkdir -p $(DIST_DIR)/include $(DIST_DIR)/lib
	cp include/sonic_rs.h $(DIST_DIR)/include/sonic_rs.h
	@if [ -f target/release/libsonic_rs_c.a ]; then \
	    cp target/release/libsonic_rs_c.a $(DIST_DIR)/lib/libsonic_rs_c.a; \
	fi
	@if [ -f target/release/sonic_rs_c.lib ]; then \
	    cp target/release/sonic_rs_c.lib $(DIST_DIR)/lib/sonic_rs_c.lib; \
	    cp target/release/sonic_rs_c.lib $(DIST_DIR)/lib/libsonic_rs_c.a; \
	fi
	@echo "==> Built embeddable artifacts in $(DIST_DIR)/"

example: dist
	$(CC) $(CFLAGS) examples/example.c $(DIST_DIR)/lib/libsonic_rs_c.a $(LDFLAGS_SYS) -o $(DIST_DIR)/example$(EXE_EXT)
	./$(DIST_DIR)/example$(EXE_EXT)

test-c: dist
	$(CC) $(CFLAGS) tests/test_c_api.c $(DIST_DIR)/lib/libsonic_rs_c.a $(LDFLAGS_SYS) -o $(DIST_DIR)/test_c_api$(EXE_EXT)
	./$(DIST_DIR)/test_c_api$(EXE_EXT)

headers:
	cbindgen --config cbindgen.toml --crate sonic-rs-c --output include/sonic_rs.h

clean:
	$(CARGO) clean
	rm -rf $(DIST_DIR)
