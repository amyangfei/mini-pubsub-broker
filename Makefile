BUILD_PATH = build
SRC_PATH = src
SRC_EXT = c
INCLUDES = -I $(SRC_PATH)/ -I $(SRC_PATH)/common/ -I $(SRC_PATH)/protocol
WARN = -Wall
CFLAGS = $(STD) $(WARN)
DCOMPILE_FLAGS = -g
COMPILE_FLAGS = -O3
release: CFLAGS += $(COMPILE_FLAGS)
debug: CFLAGS += $(DCOMPILE_FLAGS)

SOURCES = $(shell find $(SRC_PATH)/ -name '*.$(SRC_EXT)')
OBJECTS = $(SOURCES:$(SRC_PATH)/%.$(SRC_EXT)=$(BUILD_PATH)/%.o)
DEPS = $(OBJECTS:.o=.d)

all: dirs $(BUILD_PATH)/broker
.PHONY: all

$(BUILD_PATH)/broker: $(BUILD_PATH)/broker.o $(BUILD_PATH)/util.o \
	$(BUILD_PATH)/common/cJSON.o $(BUILD_PATH)/common/zmalloc.o \
	$(BUILD_PATH)/common/sds.o \
	$(BUILD_PATH)/config.o $(BUILD_PATH)/net.o $(BUILD_PATH)/event.o \
	$(BUILD_PATH)/protocol/pubcli.o $(BUILD_PATH)/protocol/subcli.o
	$(CC) $(CFLAGS) -o $@ $^ -levent -lm

$(BUILD_PATH)/%.o: $(SRC_PATH)/%.$(SRC_EXT)
	@echo "Compiling: $< -> $@"
	$(CC) $(CFLAGS) $(INCLUDES) -MP -MMD -c $< -o $@

# Macros for timing compilation
TIME_FILE = $(dir $@).$(notdir $@)_time
START_TIME = date '+%s' > $(TIME_FILE)
END_TIME = read st < $(TIME_FILE) ; \
    $(RM) $(TIME_FILE) ; \
    st=$$((`date '+%s'` - $$st - 86400)) ; \
    echo `date -u -d @$$st '+%H:%M:%S'`

.PHONY: release
release: dirs $(OBJECTS)
	@echo "Beginning release build"
	@$(START_TIME)
	@$(MAKE) all --no-print-directory
	@echo -n "Total build time: "
	@$(END_TIME)

.PHONY: debug
debug: dirs $(OBJECTS)
	@echo "Beginning debug build"
	@$(START_TIME)
	@$(MAKE) all --no-print-directory
	@echo -n "Total build time: "
	@$(END_TIME)

# Create the directories used in the build
.PHONY: dirs
dirs:
	@echo "Creating directories"
	@mkdir -p $(dir $(OBJECTS))

clean:
	rm -rf build
.PHONY: clean

