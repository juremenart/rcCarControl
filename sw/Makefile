SUBDIRS := daemon cli

# Global settings
export BASE_DIR:=$(dir $(abspath $(lastword $(MAKEFILE_LIST))))
export BIN_DIR=$(BASE_DIR)/bin/
# Object directory is actually relative for each subdirectory
export OBJ_DIR=$(BASE_DIR)/obj/

export RM=rm
export MKDIR=mkdir

all: make_dirs $(SUBDIRS)

clean:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done
	@$(RM) -rf $(BIN_DIR) $(OBJ_DIR)

make_dirs:
	@echo "Target output directory: $(BIN_DIR)"
	@$(MKDIR) -p $(BIN_DIR)
	@echo "Objects output directory: $(OBJ_DIR)"
	@$(MKDIR) -p $(OBJ_DIR)

$(SUBDIRS):
	$(MAKE) -C $@

.PHONY: all $(SUBDIRS) clean
