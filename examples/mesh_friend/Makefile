# GD32VW553 BLE Mesh Friend / LPN — Klin emit-c.
#
#   make emit KLIN=/path/to/klin/bin/klin.dart

KLIN ?= klin

.PHONY: all emit clean

all: emit

emit: out/mesh_friend.c

out/mesh_friend.c out/mesh_friend.link: mesh_friend.kl $(wildcard ../../gd32v_ble/*.kl) $(wildcard ../../gd32v_ble/*.[ch])
	@mkdir -p out
	@case "$(KLIN)" in \
	  *.dart) dart run "$(KLIN)" --emit-c mesh_friend.kl ;; \
	  *) "$(KLIN)" --emit-c mesh_friend.kl ;; \
	esac

clean:
	rm -rf out
