default: help

.PHONY: help
help:
	@echo "Targets:"
	@echo
	@echo "  tags -- regerate tags via ctags"


.PHONY: tags
tags:
	ctags -R --exclude=plugins/telemetry_plugin/lib/prometheus-cpp/3rdparty .
