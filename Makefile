# Top-level entry points. The app builds through scripts/dev-build.sh against
# the KDE snap SDK (DOCS/DECISIONS.md ADR-001).
#
#   make build     configure and build the app with tests, warnings as errors on new code
#   make test      build, then ctest
#   make lint      clang-format, clang-tidy (new code only, ADR-002), shellcheck, reuse (ADR-006)
#   make format    apply clang-format to new code
#   make screenshots  store screenshots into screenshots/ (scripts/screenshots.sh)

BUILD_DIR ?= build-sdk

# Code held to the omnidict standards. Legacy src/ and the older modules are
# left as they are until they are cleaned up (ADR-002).
STRICT_DIRS := tests $(wildcard modules/OfflineTranslation modules/AppShell)
CXX_SOURCES := $(shell find $(STRICT_DIRS) -name '*.cpp' -o -name '*.h')
TIDY_SOURCES := $(shell find $(STRICT_DIRS) -name '*.cpp')

.PHONY: all build test lint format clean screenshots

all: test

build:
	GLATE_BUILD_DIR=$(abspath $(BUILD_DIR)) \
	GLATE_CMAKE_ARGS="-DGLATE_BUILD_TESTS=ON -DGLATE_WERROR=ON $(GLATE_CMAKE_ARGS)" \
	scripts/dev-build.sh

test: build
	GLATE_BUILD_DIR=$(abspath $(BUILD_DIR)) scripts/dev-run.sh --ctest

lint: build
	clang-format --dry-run --Werror $(CXX_SOURCES)
	clang-tidy -p $(BUILD_DIR) --quiet --warnings-as-errors='*' $(TIDY_SOURCES)
	shellcheck -x scripts/*.sh
	uvx reuse lint

format:
	clang-format -i $(CXX_SOURCES)

clean:
	rm -rf $(BUILD_DIR) compile_commands.json

screenshots: build
	GLATE_BUILD_DIR=$(abspath $(BUILD_DIR)) scripts/screenshots.sh
