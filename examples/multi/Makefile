# GD32VW553 BLE multi GATT + UUID128 — Klin emit-c (ELF needs the official SDK).
#
#   make emit KLIN=/path/to/klin/bin/klin.dart

KLIN ?= klin

.PHONY: all emit clean

all: emit

emit: out/multi.c

out/multi.c out/multi.link: multi.kl $(wildcard ../../gd32v_ble/*.kl) $(wildcard ../../gd32v_ble/*.[ch])
	@mkdir -p out
	@case "$(KLIN)" in \
	  *.dart) dart run "$(KLIN)" --emit-c multi.kl ;; \
	  *) "$(KLIN)" --emit-c multi.kl ;; \
	esac

clean:
	rm -rf out
