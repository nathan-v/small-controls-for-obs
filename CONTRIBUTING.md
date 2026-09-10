# Contributing

## Code Style

C++ is formatted with clang-format 19 using the OBS style in `.clang-format`; CMake is formatted with gersemi using `.gersemirc`. CI checks both on changed files.

```bash
./build-aux/run-clang-format --check
./build-aux/run-gersemi --check
```

Drop `--check` to format in place. The clang-format wrapper insists on version 19.1.x, older or newer is rejected; on macOS, `brew install obsproject/tools/clang-format@19`.

## OBS Versions

OBS 30 and newer are supported. `buildspec.json` pins the OBS and Qt versions the plugin compiles against.

## Developer Setup

```bash
git clone https://github.com/nathan-v/small-controls-for-obs.git
cd small-controls-for-obs
cmake --preset macos          # or windows-x64, ubuntu-x86_64
cmake --build --preset macos
```

The macOS and Windows presets download prebuilt OBS and Qt dependencies into `.deps/` on first configure. Linux builds against the system packages listed in the README. Install paths per platform are in the README too.

## Testing

`tests/` holds a headless test suite that compiles the widget against a fake libobs (`tests/obs-stub.cpp`), so it runs with no OBS install. The harness is plain Qt Widgets (`tests/harness.hpp`) because the Qt that obs-deps ships has no QtTest; that keeps the suite buildable on every platform the plugin builds on. It is off by default; turn it on at configure time and run it with ctest:

```bash
cmake --preset macos -DENABLE_TESTS=ON      # or ubuntu-x86_64, windows-x64
cmake --build --preset macos
ctest --preset macos                        # or the matching preset
```

CI (`.github/workflows/ci.yml`) runs the suite on Ubuntu, macOS, and Windows for every push and pull request. Add a test for anything the suite can reach: a new frontend event, a label, a visibility rule, a click path. The stub records every `obs_frontend_*` call the widget makes and lets a test fire any frontend event, so most behaviour is a few lines to cover. If the PR is a bug fix, include a regression test.

Theme styling, real OBS event ordering, and dock placement are outside the suite. Before opening a PR that touches those, install the built plugin and check the dock in a running OBS on at least one platform, then say which platforms and OBS versions you tested in the PR description.

## Pull Request Process

1. Make sure the format checks pass
2. Make sure the test suite passes
3. Make sure the plugin builds on the platform you changed (CI builds all three)
4. Update the README if your change affects usage
5. Reference any related issues in your PR description
