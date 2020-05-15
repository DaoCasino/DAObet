# default OS; possible values: ubuntu, centos
os := ubuntu

# guess organization and project name by root directory name
root_dir := $(shell basename "$(dir $(abspath $(lastword $(MAKEFILE_LIST))))" )
org     := $(shell case "$(root_dir)" in (daobet*) echo daocasino ;; (haya*) echo mixbytes ;; esac )
project := $(shell case "$(root_dir)" in (daobet*) echo daobet    ;; (haya*) echo haya     ;; esac )

build_env_img_name := $(org)/$(project).build-env.$(os)
contracts_env_img_name := $(org)/$(project)-with-cdt.$(os)


default: help

.PHONY: help
help:
	@echo "Targets:"
	@echo
	@echo "  build-env-img     : build $(build_env_img_name) docker image"
	@echo "                      (add 'os=centos' option to build for CentOS)"
	@echo "  contracts-env-img : build $(contracts_env_img_name) docker image"
	@echo "                      (add 'os=centos' option to build for CentOS)"
	@echo
	@echo "  tags              : generate tags file via ctags"
	@echo "  clean             : remove all build artifacts (build/*)"

.PHONY: build-env-img
build-env-img:
	docker build \
		--build-arg project="$(project)" \
		--target node-build-env \
		--tag "$(build_env_img_name)" \
		-f cicd/Dockerfile.build-env.$(os) .

.PHONY: contracts-env-img
contracts-env-img:
	docker build \
		--build-arg project="$(project)" \
		--target node-with-cdt \
		--tag "$(contracts_env_img_name)" \
		-f cicd/Dockerfile.build-env.$(os) .

.PHONY: tags
tags:
	ctags -R --exclude=plugins/telemetry_plugin/lib/prometheus-cpp/3rdparty --exclude=build .

.PHONY: clean
clean:
	rm -rf build/*
