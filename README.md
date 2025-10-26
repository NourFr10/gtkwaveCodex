# GTKWave C++ Clone

This project is a Qt 5 based prototype of a professional waveform viewer implemented in C++17. It focuses on the core experience of loading signal hierarchies from real FST traces (via `fst2vcd`) or raw VCD files, browsing design scopes, and visualising waveforms with rich zooming, cursor measurements, and a studio-inspired dark theme.

## Features

- Native loading of VCD files and binary FST traces (converted on the fly through the `fst2vcd` utility shipped with GTKWave).
- Studio-inspired layout with a design browser, search-driven filtering, and a dark, high-contrast waveform canvas.
- Digital and bus waveform rendering with crisp grid lines, adaptive tick marks, and colour-coded cursor overlays.
- Mouse and keyboard interactions for zooming, panning, and placing baseline/primary cursors; toolbar shortcuts for quick resets.
- Filterable signal tree for large designs and a bundled sample trace (`test_data/sample.fst`) for quick experimentation.

## Building

The application uses CMake (3.10+) and Qt 5.9+. On a CentOS 7 environment with Qt installed:

```bash
mkdir -p build
cd build
cmake ..
cmake --build .
```

The resulting executable `gtkwave_cpp_clone` will be placed in the build directory.

> **Dependency note:** FST support uses the external `fst2vcd` helper (part of the GTKWave tool suite). Ensure it is installed and available on your `PATH`. If the converter is missing, the viewer will still open VCD files but will report a descriptive error when attempting to open an FST trace.

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
