# GD32VW553 BLE mesh / RPA — Klin emit-c (ELF needs the official SDK).
#
#   make emit KLIN=/path/to/klin/bin/klin.dart

KLIN ?= klin

.PHONY: all emit clean

all: emit

emit: out/mesh.c

out/mesh.c out/mesh.link: mesh.kl $(wildcard ../../gd32v_ble/*.kl) $(wildcard ../../gd32v_ble/*.[ch])
	@mkdir -p out
	@case "$(KLIN)" in \
	  *.dart) dart run "$(KLIN)" --emit-c mesh.kl ;; \
	  *) "$(KLIN)" --emit-c mesh.kl ;; \
	esac

clean:
	rm -rf out
