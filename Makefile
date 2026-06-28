ROOT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
BUILD_PRESET ?= dev
CONFIGURE_PRESET ?= dev
BUILD_DIR ?= $(ROOT_DIR)/build/$(BUILD_PRESET)
SANDBOX_BINARY := $(BUILD_DIR)/bin/greenfield_sandbox
LOCAL_VCPKG_ROOT := $(ROOT_DIR)/.tools/vcpkg
SOURCE_DIRS := "$(ROOT_DIR)/apps" "$(ROOT_DIR)/consumers" "$(ROOT_DIR)/engine" "$(ROOT_DIR)/templates" "$(ROOT_DIR)/tests"
SOURCE_FILES := $(shell find $(SOURCE_DIRS) \( -name '*.h' -o -name '*.hpp' -o -name '*.cpp' -o -name '*.cc' -o -name '*.cxx' \) -print)

.PHONY: bootstrap configure build run test clean format

bootstrap:
	@set -eu; \
	if [ -n "$${VCPKG_ROOT:-}" ]; then \
		vcpkg_root="$${VCPKG_ROOT}"; \
		if [ ! -f "$${vcpkg_root}/scripts/buildsystems/vcpkg.cmake" ]; then \
			echo "VCPKG_ROOT is set to '$${vcpkg_root}', but scripts/buildsystems/vcpkg.cmake was not found." >&2; \
			exit 1; \
		fi; \
	else \
		vcpkg_root="$(LOCAL_VCPKG_ROOT)"; \
		if [ ! -d "$${vcpkg_root}/.git" ]; then \
			mkdir -p "$(ROOT_DIR)/.tools"; \
			git clone --depth 1 https://github.com/microsoft/vcpkg "$${vcpkg_root}"; \
		fi; \
	fi; \
	if [ ! -x "$${vcpkg_root}/vcpkg" ]; then \
		if [ -x "$${vcpkg_root}/bootstrap-vcpkg.sh" ]; then \
			"$${vcpkg_root}/bootstrap-vcpkg.sh"; \
		else \
			echo "vcpkg is missing from '$${vcpkg_root}' and bootstrap-vcpkg.sh was not found." >&2; \
			exit 1; \
		fi; \
	fi; \
	cmake --preset "$(CONFIGURE_PRESET)"

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
