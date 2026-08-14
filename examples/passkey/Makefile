# GD32VW553 BLE passkey — Klin emit-c (ELF needs the official SDK).
#
#   make emit KLIN=/path/to/klin/bin/klin.dart

KLIN ?= klin

.PHONY: all emit clean

all: emit

emit: out/passkey.c

out/passkey.c out/passkey.link: passkey.kl $(wildcard ../../gd32v_ble/*.kl) $(wildcard ../../gd32v_ble/*.[ch])
	@mkdir -p out
	@case "$(KLIN)" in \
	  *.dart) dart run "$(KLIN)" --emit-c passkey.kl ;; \
	  *) "$(KLIN)" --emit-c passkey.kl ;; \
	esac

clean:
	rm -rf out
