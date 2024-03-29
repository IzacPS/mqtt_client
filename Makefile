# Alternative GNU Make workspace makefile autogenerated by Premake

ifndef config
  config=debug
endif

ifndef verbose
  SILENT = @
endif

ifeq ($(config),debug)
  mqtt_sub_client_config = debug

else ifeq ($(config),release)
  mqtt_sub_client_config = release

else
  $(error "invalid configuration $(config)")
endif

PROJECTS := mqtt_sub_client

.PHONY: all clean help $(PROJECTS) 

all: $(PROJECTS)

mqtt_sub_client:
ifneq (,$(mqtt_sub_client_config))
	@echo "==== Building mqtt_sub_client ($(mqtt_sub_client_config)) ===="
	@${MAKE} --no-print-directory -C . -f mqtt_sub_client.make config=$(mqtt_sub_client_config)
endif

clean:
	@${MAKE} --no-print-directory -C . -f mqtt_sub_client.make clean

help:
	@echo "Usage: make [config=name] [target]"
	@echo ""
	@echo "CONFIGURATIONS:"
	@echo "  debug"
	@echo "  release"
	@echo ""
	@echo "TARGETS:"
	@echo "   all (default)"
	@echo "   clean"
	@echo "   mqtt_sub_client"
	@echo ""
	@echo "For more information, see https://github.com/premake/premake-core/wiki"