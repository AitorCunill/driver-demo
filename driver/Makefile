ifneq ($(KERNELRELEASE),)
obj-m += sop_char.o
else
KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Wpedantic

.PHONY: all module test clean check-env intellisense

all: module test

module: check-env
	$(MAKE) -C $(KDIR) M=$(PWD) modules

test: test_sop_char

intellisense: module
	python3 "$(KDIR)/scripts/clang-tools/gen_compile_commands.py" -d "$(PWD)" -o "$(PWD)/compile_commands.json"

test_sop_char: test_sop_char.c sop_char_ioctl.h
	$(CC) $(CFLAGS) -o $@ test_sop_char.c

check-env:
	@test -f "$(KDIR)/Makefile" || { \
		echo "Error: no están los headers para $$(uname -r) en $(KDIR)"; \
		exit 1; \
	}

clean:
	$(RM) test_sop_char
	@if test -f "$(KDIR)/Makefile"; then \
		$(MAKE) -C $(KDIR) M=$(PWD) clean; \
	else \
		echo "Aviso: sin headers; se omitió la limpieza de Kbuild"; \
	fi
endif
