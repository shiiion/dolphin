# PrimeHack

[Discord](https://discord.gg/Gc2HcPH)

PrimeHack is a Dolphin Emulator fork for the Metroid Prime series games available for Windows,
Linux, macOS, and Android\*. It's licensed under the terms
of the GNU General Public License, version 2 or later (GPLv2+).
_\*The Android build is not officially developed or built and is provided by the community._

Please read the [wiki](https://github.com/shiiion/dolphin/wiki) before using PrimeHack.

## System Requirements

### Desktop

* OS
    * Windows (10 1903 or higher).
    * Linux.
    * macOS (11.0 Big Sur or higher).
    * Unix-like systems other than Linux are not officially supported but might work.
* Processor
    * A CPU with SSE2 support.
    * A modern CPU (3 GHz and Dual Core, not older than 2008) is highly recommended.
* Graphics
    * A reasonably modern graphics card (Direct3D 11.1 / OpenGL 3.3).
    * A graphics card that supports Direct3D 11.1 / OpenGL 4.4 is recommended.

### Android

* OS
    * Android (5.0 Lollipop or higher).
* Processor
    * A processor with support for 64-bit applications (either ARMv8 or x86-64).
* Graphics
    * A graphics processor that supports OpenGL ES 3.0 or higher. Performance varies heavily with [driver quality](https://dolphin-emu.org/blog/2013/09/26/dolphin-emulator-and-opengl-drivers-hall-fameshame/).
    * A graphics processor that supports standard desktop OpenGL features is recommended for best performance.

PrimeHack can only be installed on devices that satisfy the above requirements. Attempting to install on an unsupported device will fail and display an error message.

## Building for Windows

Use the solution file `Source/dolphin-emu.sln` to build PrimeHack on Windows.
PrimeHack targets the latest MSVC shipped with Visual Studio or Build Tools.
Other compilers might be able to build PrimeHack on Windows but have not been
tested and are not recommended to be used. Git and latest Windows SDK must be
installed when building.

Make sure to pull submodules before building:
```sh
git submodule update --init --recursive
```

The "Release" solution configuration includes performance optimizations for the best user experience but complicates debugging PrimeHack.
The "Debug" solution configuration is significantly slower, more verbose and less permissive but makes debugging PrimeHack easier.

## Building for Linux and macOS

PrimeHack requires [CMake](https://cmake.org/) for systems other than Windows. 
You need a recent version of GCC or Clang with decent c++20 support. CMake will
inform you if your compiler is too old.
Many libraries are bundled with PrimeHack and used if they're not installed on 
your system. CMake will inform you if a bundled library is used or if you need
to install any missing packages yourself. You may refer to the [wiki](https://github.com/dolphin-emu/dolphin/wiki/Building-for-Linux) for more information.

Make sure to pull submodules before building:
```sh
git submodule update --init --recursive
```

### macOS Build Steps:

> **Building on a current macOS (Xcode 26+, CMake 4+)?** The plain steps below
> may need extra flags on a modern toolchain. See
> [`BUILDING_MACOS_MODERN.md`](BUILDING_MACOS_MODERN.md) (Español + English).

A binary supporting a single architecture can be built using the following steps: 

1. `mkdir build`
2. `cd build`
3. `cmake ..`
4. `make -j $(sysctl -n hw.logicalcpu)`

An application bundle will be created in `./Binaries`.

A script is also provided to build universal binaries supporting both x64 and ARM in the same
application bundle using the following steps:

1. `mkdir build`
2. `cd build`
3. `python ../BuildMacOSUniversalBinary.py`
4. Universal binaries will be available in the `universal` folder

Doing this is more complex as it requires installation of library dependencies for both x64 and ARM (or universal library
equivalents) and may require specifying additional arguments to point to relevant library locations. 
Execute BuildMacOSUniversalBinary.py --help for more details.  

### Linux Global Build Steps:

To install to your system.

1. `mkdir build`
2. `cd build`
3. `cmake ..`
4. `make -j $(nproc)`
5. `sudo make install`

### Linux Local Build Steps:

Useful for development as root access is not required.

1. `mkdir Build`
2. `cd Build`
3. `cmake .. -DLINUX_LOCAL_DEV=true`
4. `make -j $(nproc)`
5. `ln -s ../../Data/Sys Binaries/`

### Linux Portable Build Steps:

Can be stored on external storage and used on different Linux systems.
Or useful for having multiple distinct PrimeHack setups for testing/development/TAS.

1. `mkdir Build`
2. `cd Build`
3. `cmake .. -DLINUX_LOCAL_DEV=true`
4. `make -j $(nproc)`
5. `cp -r ../Data/Sys/ Binaries/`
6. `touch Binaries/portable.txt`

## Building for Android

These instructions assume familiarity with Android development. If you do not have an
Android dev environment set up, see [AndroidSetup.md](AndroidSetup.md).

Make sure to pull submodules before building:
```sh
git submodule update --init --recursive
```

If using Android Studio, import the Gradle project located in `./Source/Android`.

Android apps are compiled using a build system called Gradle. PrimeHack's native component,
however, is compiled using CMake. The Gradle script will attempt to run a CMake build
automatically while building the Java code.

## Uninstalling

On Windows, simply remove the extracted directory, unless it was installed with the NSIS installer,
in which case you can uninstall PrimeHack like any other Windows application.

Linux users can run `cat install_manifest.txt | xargs -d '\n' rm` as root from the build directory
to uninstall PrimeHack from their system.

macOS users can simply delete PrimeHack.app to uninstall it.

Additionally, you'll want to remove the global user directory if you don't plan on reinstalling PrimeHack.

## Command Line Usage

```
Usage: PrimeHack.exe [options]... [FILE]...

Options:
  --version             show program's version number and exit
  -h, --help            show this help message and exit
  -u USER, --user=USER  User folder path
  -m MOVIE, --movie=MOVIE
                        Play a movie file
  -e <file>, --exec=<file>
                        Load the specified file
  -n <16-character ASCII title ID>, --nand_title=<16-character ASCII title ID>
                        Launch a NAND title
  -C <System>.<Section>.<Key>=<Value>, --config=<System>.<Section>.<Key>=<Value>
                        Set a configuration option
  -s <file>, --save_state=<file>
                        Load the initial save state
  -d, --debugger        Show the debugger pane and additional View menu options
  -l, --logger          Open the logger
  -b, --batch           Run PrimeHack without the user interface (Requires
                        --exec or --nand-title)
  -c, --confirm         Set Confirm on Stop
  -v VIDEO_BACKEND, --video_backend=VIDEO_BACKEND
                        Specify a video backend
  -a AUDIO_EMULATION, --audio_emulation=AUDIO_EMULATION
                        Choose audio emulation from [HLE|LLE]
```

Available DSP emulation engines are HLE (High Level Emulation) and
LLE (Low Level Emulation). HLE is faster but less accurate whereas
LLE is slower but close to perfect. Note that LLE has two submodes (Interpreter and Recompiler)
but they cannot be selected from the command line.

Available video backends are "D3D" and "D3D12" (they are only available on Windows), "OGL", and "Vulkan".
There's also "Null", which will not render anything, and
"Software Renderer", which uses the CPU for rendering and
is intended for debugging purposes only.

## DolphinTool Usage
```
usage: dolphin-tool COMMAND -h

commands supported: [convert, verify, header, extract]
```

```
Usage: convert [options]... [FILE]...

Options:
  -h, --help            show this help message and exit
  -u USER, --user=USER  User folder path, required for temporary processing
                        files.Will be automatically created if this option is
                        not set.
  -i FILE, --input=FILE
                        Path to disc image FILE.
  -o FILE, --output=FILE
                        Path to the destination FILE.
  -f FORMAT, --format=FORMAT
                        Container format to use. Default is RVZ. [iso|gcz|wia|rvz]
  -s, --scrub           Scrub junk data as part of conversion.
  -b BLOCK_SIZE, --block_size=BLOCK_SIZE
                        Block size for GCZ/WIA/RVZ formats, as an integer.
                        Suggested value for RVZ: 131072 (128 KiB)
  -c COMPRESSION, --compression=COMPRESSION
                        Compression method to use when converting to WIA/RVZ.
                        Suggested value for RVZ: zstd [none|zstd|bzip|lzma|lzma2]
  -l COMPRESSION_LEVEL, --compression_level=COMPRESSION_LEVEL
                        Level of compression for the selected method. Ignored
                        if 'none'. Suggested value for zstd: 5
```

```
Usage: verify [options]...

Options:
  -h, --help            show this help message and exit
  -u USER, --user=USER  User folder path, required for temporary processing
                        files.Will be automatically created if this option is
                        not set.
  -i FILE, --input=FILE
                        Path to disc image FILE.
  -a ALGORITHM, --algorithm=ALGORITHM
                        Optional. Compute and print the digest using the
                        selected algorithm, then exit. [crc32|md5|sha1|rchash]
```

```
Usage: header [options]...

Options:
  -h, --help            show this help message and exit
  -i FILE, --input=FILE
                        Path to disc image FILE.
  -b, --block_size      Optional. Print the block size of GCZ/WIA/RVZ formats,
then exit.
  -c, --compression     Optional. Print the compression method of GCZ/WIA/RVZ
                        formats, then exit.
  -l, --compression_level
                        Optional. Print the level of compression for WIA/RVZ
                        formats, then exit.
```

```
Usage: extract [options]...

Options:
  -h, --help            show this help message and exit
  -i FILE, --input=FILE
                        Path to disc image FILE.
  -o FOLDER, --output=FOLDER
                        Path to the destination FOLDER.
  -p PARTITION, --partition=PARTITION
                        Which specific partition you want to extract.
  -s SINGLE, --single=SINGLE
                        Which specific file/directory you want to extract.
  -l, --list            List all files in volume/partition. Will print the
                        directory/file specified with --single if defined.
  -q, --quiet           Mute all messages except for errors.
  -g, --gameonly        Only extracts the DATA partition.
```
