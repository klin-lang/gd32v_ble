# GD32VW553 BLE privacy / RPA — Klin emit-c (ELF needs the official SDK).
#
#   make emit KLIN=/path/to/klin/bin/klin.dart

KLIN ?= klin

.PHONY: all emit clean

all: emit

emit: out/privacy.c

out/privacy.c out/privacy.link: privacy.kl $(wildcard ../../gd32v_ble/*.kl) $(wildcard ../../gd32v_ble/*.[ch])
	@mkdir -p out
	@case "$(KLIN)" in \
	  *.dart) dart run "$(KLIN)" --emit-c privacy.kl ;; \
	  *) "$(KLIN)" --emit-c privacy.kl ;; \
	esac

clean:
	rm -rf out
