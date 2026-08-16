# GD32VW553 BLE Mesh interactive OOB — Klin emit-c.
#
#   make emit KLIN=/path/to/klin/bin/klin.dart

KLIN ?= klin

.PHONY: all emit clean

all: emit

emit: out/mesh_oob.c

out/mesh_oob.c out/mesh_oob.link: mesh_oob.kl $(wildcard ../../gd32v_ble/*.kl) $(wildcard ../../gd32v_ble/*.[ch])
	@mkdir -p out
	@case "$(KLIN)" in \
	  *.dart) dart run "$(KLIN)" --emit-c mesh_oob.kl ;; \
	  *) "$(KLIN)" --emit-c mesh_oob.kl ;; \
	esac

clean:
	rm -rf out
