# C++ quality tooling

This repository defines a common, reviewable baseline for C++20 formatting and
static analysis. The configuration is tooling-only: it does not change runtime
code, generated providers, active aircraft profiles, release contents or flight
approval.

## Scope and current enforcement

- `.clang-format` defines the style for new or deliberately edited C/C++ code.
- `.clang-tidy` enables conservative `bugprone-*`, `cppcoreguidelines-*` and
  `performance-*` checks. A few high-noise checks are disabled explicitly.
- `.pre-commit-config.yaml` checks formatting only on staged C/C++ files and
  verifies that generated project-status files are current.
- clang-tidy is a **manual** pre-commit stage until the existing codebase has a
  reviewed warning baseline. It is not a blocking CI gate in this change.

Do not run a repository-wide formatting rewrite as part of an unrelated change.
Generated provider tables under `include/efis/providers/*_table.hpp` are excluded
from pre-commit processing and must continue to be produced by their generators.

## Prerequisites

Install compatible releases of clang-format, clang-tidy and pre-commit on the
development workstation. The executables must be available on `PATH`. Configure
the normal development preset before running clang-tidy so that the compilation
database exists:

```sh
cmake --preset dev
```

## Local use

Install the default hooks once per clone:

```sh
pre-commit install
```

Run the default checks on the files staged for the next commit:

```sh
pre-commit run
```

`pre-commit run --all-files` is useful for measuring existing formatting debt,
but it is not a requirement of this initial baseline and may report legacy
files that have not yet been deliberately reformatted.

Run clang-tidy explicitly after `build/dev/compile_commands.json` has been
generated:

```sh
pre-commit run clang-tidy --all-files --hook-stage manual
```

To inspect formatting without changing files:

```sh
clang-format --dry-run --Werror path/to/changed_file.cpp
```

Apply formatting only to files intentionally in scope for the current branch,
then inspect the diff before committing. A future, dedicated change may establish
and review a whole-tree baseline; this configuration does not do that.
