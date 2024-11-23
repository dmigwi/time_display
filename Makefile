# Directories
WORKING_DIR = .

# Toolchain
TARGET_EXEC := arduino-cli

# Targets
UPLOAD_OP = upload
COMPILE_OP = compile

# Flags
OBJ := $(wildcard $(WORKING_DIR)/*.ino)
CONFIG_FILE = --config-file arduino-cli.yaml
JQ_COMMAND = ".detected_ports | . [] |  select( .matching_boards | length > 0) | .port.address" --raw-output
BOARD_PORT = $(shell $(TARGET_EXEC) board list --json | jq -c $(JQ_COMMAND))
PROFILE = leonardo

# Compiles the code in the working directory.
$(COMPILE_OP): $(OBJ)
	@echo "==> Compile the code on in $(OBJ) \n"
	$(TARGET_EXEC) compile $(CONFIG_FILE) --profile $(PROFILE)

# Builds and Flashes the code in the working directory to the arduino board.
$(UPLOAD_OP):  $(OBJ)
	@echo "==> Uploading the code on to Arduino on port $(BOARD_PORT) \n"
	$(TARGET_EXEC) upload -p $(BOARD_PORT) $(CONFIG_FILE) $(WORKING_DIR)
