# Build notes

How this fork is built and tested, and what the test suite looked like before
any change was made. Every later branch is verified with the commands below,
and a test that fails here must not be attributed to a later change.

## Baseline

| | |
|---|---|
| Date | 2026-10-04 |
| Commit | `534d3b4a4` (`upstream/master`, "Merge pull request #1277 from saschbe/docs/improve-testing-guide") |
| Machine | macOS 26.6.2, Apple Silicon, 10 cores |
| Compiler | Apple clang 21.0.0 (clang-2100.3.34.2) |
| CMake / generator | CMake 4.4.3, Ninja |
| Qt | 6.11.2 (Homebrew `qt`) |
| KDE Frameworks | **off** (`-DBUILD_WITH_KF=OFF`) — see below |

## Commands

These follow upstream's testing guide in `CONTRIBUTING.md` ("Build the
tests", "Run the tests with CTest", "Tests outside the CTest suite"), which
is the reference if anything here drifts. From the repository root, in a
clean tree:

```sh
git submodule update --init --recursive
cmake -S . -B build-m0-qt6 -G Ninja \
      -DCMAKE_BUILD_TYPE=Debug \
      -DPACKAGE_TESTS=ON \
      -DQT_VERSION_MAJOR=6 \
      -DBUILD_WITH_KF=OFF \
      -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build-m0-qt6 --parallel

ctest --test-dir build-m0-qt6 --output-on-failure --timeout 300
./build-m0-qt6/tests/catch/C_unittests
python3 misc/qet-mcp/test_qet_mcp.py
```

Configure takes about 30 s, the build about 2.5 min, the CTest run about
3.5 min.

### Why KDE Frameworks are off

With the default `BUILD_WITH_KF=ON`, configure fetches `kcoreaddons` and then
fails: `Could NOT find ECM (missing: ECM_DIR)` — Extra CMake Modules ≥ 6.10.0
is not installed. `-DBUILD_WITH_KF=OFF` is the fallback documented in
`INSTALL.md` and `CONTRIBUTING.md`. A change that touches code behind
`BUILD_WITH_KF` needs a build with KF enabled before it goes upstream.

### Qt 5

Out of scope. This fork is built and tested with Qt 6 only.

## Results before any change

### Build

Succeeds. 7 compiler warnings, all in existing code:

| Count | Location | Warning |
|---|---|---|
| 5 | `sources/qet.cpp:540` | ignoring `[[nodiscard]]` return value (`-Wunused-result`) |
| 1 | `sources/elementsmover.cpp:87` | assigning field to itself (`-Wself-assign-field`) |
| 1 | `sources/editor/UndoCommand/openelmtcommand.cpp:76` | `@TODO` pragma message |

### CTest — 56 of 57 pass

| Test | Result |
|---|---|
| 56 tests | pass |
| `tst_elementautonumids` | **fails, reproducibly** (two runs) |

The failing case is one of 14 in that test:

```text
FAIL!  : tst_elementautonumids::duplicatesAreNotNumberedWhenSwitchedOff()
         'erased.value(QStringLiteral("label")).toString().isEmpty()' returned FALSE.
   Loc: [tests/qttest/tst_elementautonumids.cpp(707)]
Totals: 13 passed, 1 failed, 0 skipped
```

The test was added upstream on 2026-10-04. Not investigated; it may be
macOS-specific (the case injects `autonumber-pasted-elements=false` through a
preferences file). It is a pre-existing failure.

`modal_quit_regression` is not registered on macOS (Linux only).

### Catch2 (`C_unittests`, outside CTest) — pass

`All tests passed (59 assertions in 4 test cases)`

### Python MCP suite (outside CTest) — not established

Run inside a sandbox that forbids executing fixture scripts and limits socket
path length: 401 tests, 127 skipped (integration tests, no binary given),
5 errors, all `PermissionError` from the sandbox, not from QElectroTech.
**Re-run in a normal terminal** and replace this paragraph with the result.

### Not run

- `tests/ipc-regression/run.sh` — needs Xvfb, Openbox and xdotool (Linux)
- `modal_quit_regression` — Linux only
