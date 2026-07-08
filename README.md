# quader-godot

Godot 4 GDExtension project for Quader editor integration.

This repository was initialized from `godot-cpp-template` and keeps the template's
sample `ExampleClass` as a build smoke test until Quader-specific bindings are
added.

## Contents

- C++ GDExtension source in [src/](./src/).
- A minimal Godot test project in [project/](./project).
- `godot-cpp` as a submodule in [godot-cpp/](./godot-cpp).
- SCons and CMake build entry points.

## Setup

Initialize the Godot C++ bindings submodule after cloning:

```shell
git submodule update --init
```

The extension is configured as:

- library name: `quader_godot`
- extension descriptor: `project/bin/quader_godot.gdextension`
- entry symbol: `quader_godot_library_init`

## Build

Build with SCons:

```shell
scons
```

Or configure with CMake if your environment uses the CMake path:

```shell
cmake -S . -B build
cmake --build build
```

## Test Project

Open [project/](./project) in Godot 4. The current sample scene still instantiates
the template `ExampleClass` and prints a type value to the Godot console.
