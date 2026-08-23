# Library build test

This example is the development build for `ESP32_UtilityFunctions`. It lets you compile and debug the library from VS Code with PioArduino  while the library source remains in the repository root.

## Why the local link is needed

PioArduino normally treats the project source directory as the application and resolves libraries from `lib_deps`. 
During library development, the application in this example must also use the library currently checked out in this repository. 

so to keep the lib fins in project/src folder and the MAIN file in another folder OUT of the lib we use the HARD link to an exmple main.cpp in a folder for /example/LIB_Build_test/src/Main.cpp file

Then we can use the src_dir to point to the latest folder for the lib testing and dev in teh same vs code workspace window. 

The root `PioArduino.ini` solves this with:

```ini
[PioArduino]
src_dir = examples/LIB_bild_test/src

[env:esp32s3-n16r8-USBOTG]
lib_deps =
    symlink://.
```

`symlink://.` tells PioArduino to register the repository root as a local library dependency. Changes made in `src/UtilityFunctions.cpp`, `src/UtilityFunctions.h`, `src/WebLogPrint.cpp`, or `src/WebLogPrint.h` are therefore used by the example on the next build without publishing or downloading a new package.

## Directory layout

```text
ESP32_UtilityFunctions/
├── src/                         # Library implementation and public headers
├── examples/
│   └── LIB_bild_test/
│       ├── src/                 # Example application source
│       └── README.md
└── PioArduino.ini               # Development build configuration
```

The example application is in `examples/LIB_bild_test/src/main.cpp`. The `src_dir` setting makes that directory the application source for the root PioArduino project. The local library link still points to the repository root, where `library.json` and the library source files are located.

## Build from VS Code

1. Open the repository folder in VS Code.
2. Install the PioArduino IDE extension if it is not already installed.
3. Open the PioArduino sidebar and select **Build**, or run `PioArduino: Build` from the Command Palette.
4. Select the `esp32s3-n16r8-USBOTG` environment when prompted.

The same build can be started from the repository root with:

```text
pio run
```

This configuration uses the pioarduino ESP32 platform, ESP-IDF plus Arduino frameworks, the custom ESP32-S3 board definition, and the 16 MB partition table from the repository root.

## Developing the library

Edit the library files under the root `src/` directory, then build the `LIB_bild_test` example again. Because `symlink://.` points to the current checkout, the test application compiles against those local edits.

Keep `symlink://.` enabled while working on the library. When compiling a standalone application that should consume the released library, remove or comment out the local link and use the versioned GitHub dependency instead:

```ini
lib_deps =
    https://github.com/synapse-2/ESP32_UtilityFunctions.git#1.0.0
```

Do not use both the local symlink and the released dependency in the same environment; that can create duplicate or conflicting library resolutions.

## Windows note

PioArduino creates and manages the local link from the `symlink://.` dependency. If Windows prevents link creation, enable Developer Mode or run VS Code with an account that has permission to create symbolic links, then clean and rebuild the project.
