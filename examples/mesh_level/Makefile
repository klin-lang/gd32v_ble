# GD32VW553 BLE Mesh Gen Level + vendor — Klin emit-c.
#
#   make emit KLIN=/path/to/klin/bin/klin.dart

KLIN ?= klin

.PHONY: all emit clean

all: emit

emit: out/mesh_level.c

out/mesh_level.c out/mesh_level.link: mesh_level.kl $(wildcard ../../gd32v_ble/*.kl) $(wildcard ../../gd32v_ble/*.[ch])
	@mkdir -p out
	@case "$(KLIN)" in \
	  *.dart) dart run "$(KLIN)" --emit-c mesh_level.kl ;; \
	  *) "$(KLIN)" --emit-c mesh_level.kl ;; \
	esac

clean:
	rm -rf out
