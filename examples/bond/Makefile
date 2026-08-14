# GD32VW553 BLE Just Works bonding — Klin emit-c (ELF needs the official SDK).
#
#   make emit KLIN=/path/to/klin/bin/klin.dart

KLIN ?= klin

.PHONY: all emit clean

all: emit

emit: out/bond.c

out/bond.c out/bond.link: bond.kl $(wildcard ../../gd32v_ble/*.kl) $(wildcard ../../gd32v_ble/*.[ch])
	@mkdir -p out
	@case "$(KLIN)" in \
	  *.dart) dart run "$(KLIN)" --emit-c bond.kl ;; \
	  *) "$(KLIN)" --emit-c bond.kl ;; \
	esac

clean:
	rm -rf out
