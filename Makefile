ROOT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
BUILD_PRESET ?= dev
CONFIGURE_PRESET ?= dev
BUILD_DIR ?= $(ROOT_DIR)/build/$(BUILD_PRESET)
SANDBOX_BINARY := $(BUILD_DIR)/bin/greenfield_sandbox
SOURCE_FILES := $(shell find "$(ROOT_DIR)" -path "$(ROOT_DIR)/build" -prune -o \( -name '*.h' -o -name '*.hpp' -o -name '*.cpp' -o -name '*.cc' -o -name '*.cxx' \) -print)

.PHONY: bootstrap configure build run test clean format

bootstrap:
	cmake -S "$(ROOT_DIR)" -B "$(BUILD_DIR)" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE="$(ROOT_DIR)/cmake/vcpkg-toolchain.cmake"

configure:
	cmake --preset $(CONFIGURE_PRESET)

build: configure
	cmake --build --preset $(BUILD_PRESET)

run: build
	"$(SANDBOX_BINARY)"

test: build
	ctest --preset $(BUILD_PRESET) --output-on-failure

clean:
	rm -rf "$(ROOT_DIR)/build"

format:
	@if command -v clang-format >/dev/null 2>&1; then \
		if [ -n "$(SOURCE_FILES)" ]; then \
			clang-format -i $(SOURCE_FILES); \
		fi; \
	else \
		echo "clang-format is not installed."; \
	fi
