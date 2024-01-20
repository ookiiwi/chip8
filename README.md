# chip8

CHIP-8 emulator. My goal is to get a first hand on emulation and also to get deeper knowledge about C/C++.
Check out [chipper](./CHIPPER/CHIPPER.DOC) to build your own roms.

****** SCHIP GAMES NOT SUPPORTED YET !!! ******

## Dependencies

- [SDL2](https://www.libsdl.org/). Check out the [installation guide](https://wiki.libsdl.org/SDL2/Installation) or [brew](https://formulae.brew.sh/formula/sdl2#default)
- [ImGui](https://github.com/ocornut/imgui)
- [inih](https://github.com/benhoyt/inih)

## Build

```sh
$ git clone --recurse-submodules -j8 https://github.com/ookiiwi/chip8.git
$ cd chip8
$ mkdir build
$ cmake ..
$ cmake --build .
```

## Run

```sh
$ ./build/chip8 ./GAMES/PONG    # or run <EXEC_PATH> to get usage e.g. ./chip8
```

## Configuration

A configuration file is used to toggle options (e.g. framerate, clock speed, ...) for a specific game. See [default config](./GAMES/config.cfg).

## External resources

- [SDL2](https://www.libsdl.org/) is under the [Zlib license](https://github.com/libsdl-org/SDL/blob/main/LICENSE.txt)
- [ImGui](https://github.com/ocornut/imgui) is under the [MIT license](https://github.com/ocornut/imgui/blob/master/LICENSE.txt)
- [inih](https://github.com/benhoyt/inih) is under the [New BSD license](https://github.com/benhoyt/inih/blob/master/LICENSE.txt)
- [codeslinger](http://www.codeslinger.co.uk/pages/projects/chip8.html) was a precious resources for me and especially to better understand how CHIP-8 instructions work.
- David Winter's [CHIP-8 emulator](https://www.pong-story.com/chip8/)