# GTKWave C++ Clone

This project is a Qt 5 based prototype of a GTKWave-like waveform viewer implemented in C++17. It focuses on the core experience of loading signal hierarchies from FST-style data, browsing signals, and visualising waveforms with basic zooming and cursor measurements.

## Features

- Load pseudo-FST files containing hierarchical scopes, signal metadata, and value changes.
- Tree-based browser for navigating scopes and double-clicking signals to display them in the waveform panel.
- Digital and bus waveform rendering with cursor overlays and grid lines.
- Mouse and keyboard interactions for zooming, panning, and placing cursors.
- Simple sample dataset (`test_data/sample.fst`) for quick experimentation.

> **Note:** This prototype implements a lightweight text-based parser that mimics a subset of the FST format to keep the project self-contained. Hooking the UI up to a real FST reader is a matter of adapting `fst::SimpleFstReader` to a full parser.

## Building

The application uses CMake (3.10+) and Qt 5.9+. On a CentOS 7 environment with Qt installed:

```bash
mkdir -p build
cd build
cmake ..
cmake --build .
```

The resulting executable `gtkwave_cpp_clone` will be placed in the build directory.

### Running

```bash
./gtkwave_cpp_clone ../test_data/sample.fst
```

You can also start the application without arguments and open a file from the **File → Open FST** menu.

## Project Layout

```
├── CMakeLists.txt
├── include/
│   ├── main_window.h
│   ├── signal_tree.h
│   ├── simple_fst_reader.h
│   └── waveform_view.h
├── src/
│   ├── main.cpp
│   ├── fst/
│   │   └── simple_fst_reader.cpp
│   └── ui/
│       ├── main_window.cpp
│       ├── signal_tree.cpp
│       └── waveform_view.cpp
└── test_data/
    └── sample.fst
```

## Next Steps

- Replace the pseudo-FST reader with a complete parser or bind to an existing FST library.
- Persist workspace layouts and signal selections.
- Add advanced waveform annotations (markers, measurement windows, etc.).
- Expand testing with larger datasets to exercise performance optimisations.
