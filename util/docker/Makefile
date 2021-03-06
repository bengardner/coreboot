##
## This file is part of the coreboot project.
##
## Copyright (C) 2016 Google, Inc.
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; version 2 of the License.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
export top=$(abspath $(CURDIR)/../..)
export crossgcc_version=$(shell $(top)/util/crossgcc/buildgcc --version | grep 'cross toolchain' | sed 's/^.*\sv//' | sed 's/\s.*$$//')
export DOCKER:=$(shell env sh -c "command -v docker")

test-docker:
	$(if $(DOCKER),,\
		$(warning Docker command not found.  Please install docker) \
		$(warning https://docs.docker.com/engine/installation ) \
		$(error halting))

test-docker-login: test-docker
	$(if $(shell if [ ! -f ~/.docker/config.json ]; then \
		echo "docker authentication file not found"; fi), \
		$(error Docker authentication file not found.  Run 'docker login'))

coreboot-sdk: test-docker
	$(DOCKER) build -t coreboot/coreboot-sdk:$(crossgcc_version) coreboot-sdk

upload-coreboot-sdk: test-docker-login
	$(DOCKER) push coreboot/coreboot-sdk:$(crossgcc_version)

coreboot-jenkins-node: test-docker
	$(DOCKER) build -t coreboot/coreboot-jenkins-node:$(crossgcc_version) coreboot-jenkins-node

upload-coreboot-jenkins-node: test-docker-login
	$(DOCKER) push coreboot/coreboot-jenkins-node:$(crossgcc_version)

docker-killall: test-docker
	@if [ -n "$$($(DOCKER) ps | grep 'coreboot')" ]; then \
		$(DOCKER) kill $$($(DOCKER) ps | grep 'coreboot' | cut -f1 -d ' '); \
	fi

clean-coreboot-containers: docker-killall
	$(DOCKER) rm $(docker ps -a | grep 'coreboot' | sed 's|\s.*$||')

clean-coreboot-images: docker-killall
	$(DOCKER) rmi $(docker images | grep coreboot | sed 's|^\S\+\s\+\S\+\s\+||' | sed 's|\s.*$||')

docker-build-coreboot: test-docker
	$(DOCKER) run -u root -it -v $(top):/home/coreboot/coreboot \
		--rm coreboot/coreboot-sdk:$(crossgcc_version) \
		/bin/bash -c "cd /home/coreboot/coreboot && \
		make clean && \
		make $(BUILD_CMD)"
	@echo "Enter root password to chown files to $$(whoami):$$(id -gn $$(whoami))"
	@echo "Exiting now will leave built files owned by root"
	sudo chown -R $$(whoami):$$(id -gn $$(whoami)) $(top)

docker-abuild: test-docker
	$(DOCKER) run -u root -it -v $(top):/home/coreboot/coreboot \
		--rm coreboot/coreboot-sdk:$(crossgcc_version) \
		/bin/bash -c "cd /home/coreboot/coreboot && \
		make clean && \
		util/abuild/abuild $(ABUILD_ARGS)"
	@echo "Enter root password to chown files to $$(whoami):$$(id -gn $$(whoami))"
	@echo "Exiting now will leave built files owned by root"
	sudo chown -R $$(whoami):$$(id -gn $$(whoami)) $(top)

help:
	@echo "Commands for working with docker images:"
	@echo "  coreboot-sdk                 - Build coreboot-sdk container"
	@echo "  upload-coreboot-sdk          - Upload coreboot-sdk to hub.docker.com"
	@echo "  coreboot-jenkins-node        - Build coreboot-jenkins-node container"
	@echo "  upload-coreboot-jenkins-node - Upload coreboot-jenkins-node to hub.docker.com"
	@echo "  clean-coreboot-containers    - remove all docker coreboot containers"
	@echo "  clean-coreboot-images        - remove all docker coreboot images"
	@echo
	@echo "Commands for using docker images"
	@echo "  docker-build-coreboot <BUILD_CMD=target>  - Build coreboot under coreboot-sdk"
	@echo "  docker-abuild <ABUILD_ARGS='-a -B'>       - Run abuild under coreboot-sdk"

.PHONY: test-docker test-docker-login
.PHONY: coreboot-jenkins-node upload-coreboot-jenkins-node
.PHONY: coreboot-sdk upload-coreboot-sdk
.PHONY: clean-coreboot-containers clean-coreboot-images
.PHONY: docker-abuild
.PHONY: help
