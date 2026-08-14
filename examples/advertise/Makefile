# GD32VW553 BLE advertise — Klin emit-c (ELF needs the official SDK).
#
#   make emit KLIN=/path/to/klin/bin/klin.dart

KLIN ?= klin

.PHONY: all emit clean

all: emit

emit: out/advertise.c

out/advertise.c out/advertise.link: advertise.kl $(wildcard ../../gd32v_ble/*.kl) $(wildcard ../../gd32v_ble/*.[ch])
	@mkdir -p out
	@case "$(KLIN)" in \
	  *.dart) dart run "$(KLIN)" --emit-c advertise.kl ;; \
	  *) "$(KLIN)" --emit-c advertise.kl ;; \
	esac

clean:
	rm -rf out
