# GD32VW553 BLE GATT client — Klin emit-c (ELF needs the official SDK).
#
#   make emit KLIN=/path/to/klin/bin/klin.dart

KLIN ?= klin

.PHONY: all emit clean

all: emit

emit: out/gattc.c

out/gattc.c out/gattc.link: gattc.kl $(wildcard ../../gd32v_ble/*.kl) $(wildcard ../../gd32v_ble/*.[ch])
	@mkdir -p out
	@case "$(KLIN)" in \
	  *.dart) dart run "$(KLIN)" --emit-c gattc.kl ;; \
	  *) "$(KLIN)" --emit-c gattc.kl ;; \
	esac

clean:
	rm -rf out
