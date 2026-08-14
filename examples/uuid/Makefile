# GD32VW553 BLE custom UUID16 — Klin emit-c (ELF needs the official SDK).
#
#   make emit KLIN=/path/to/klin/bin/klin.dart

KLIN ?= klin

.PHONY: all emit clean

all: emit

emit: out/uuid.c

out/uuid.c out/uuid.link: uuid.kl $(wildcard ../../gd32v_ble/*.kl) $(wildcard ../../gd32v_ble/*.[ch])
	@mkdir -p out
	@case "$(KLIN)" in \
	  *.dart) dart run "$(KLIN)" --emit-c uuid.kl ;; \
	  *) "$(KLIN)" --emit-c uuid.kl ;; \
	esac

clean:
	rm -rf out
