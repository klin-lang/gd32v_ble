# GD32VW553 BLE Mesh provisioner — Klin emit-c (ELF needs SDK mesh + BLE_MAX + CDB).
#
#   make emit KLIN=/path/to/klin/bin/klin.dart

KLIN ?= klin

.PHONY: all emit clean

all: emit

emit: out/mesh_prov.c

out/mesh_prov.c out/mesh_prov.link: mesh_prov.kl $(wildcard ../../gd32v_ble/*.kl) $(wildcard ../../gd32v_ble/*.[ch])
	@mkdir -p out
	@case "$(KLIN)" in \
	  *.dart) dart run "$(KLIN)" --emit-c mesh_prov.kl ;; \
	  *) "$(KLIN)" --emit-c mesh_prov.kl ;; \
	esac

clean:
	rm -rf out
