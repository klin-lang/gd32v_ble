# GD32VW553 BLE central scan — Klin emit-c (ELF needs the official SDK).
#
#   make emit KLIN=/path/to/klin/bin/klin.dart

KLIN ?= klin

.PHONY: all emit clean

all: emit

emit: out/scan.c

out/scan.c out/scan.link: scan.kl $(wildcard ../../gd32v_ble/*.kl) $(wildcard ../../gd32v_ble/*.[ch])
	@mkdir -p out
	@case "$(KLIN)" in \
	  *.dart) dart run "$(KLIN)" --emit-c scan.kl ;; \
	  *) "$(KLIN)" --emit-c scan.kl ;; \
	esac

clean:
	rm -rf out
