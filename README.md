# chip8

CHIP-8 emulator written in C. My goal is to get a first hand on emulation and also to get deeper knowledge about C.

## Dependencies
- [SDL2](https://www.libsdl.org/)
- [ImGui](https://github.com/ocornut/imgui)
- [inih](https://github.com/benhoyt/inih)

## Build
```sh
$ git clone https://github.com/ookiiwi/chip8.git
$ cd chip8
$ mkdir build
$ cmake ..
$ cmake --build .
```

## External resources
- [SDL2](https://www.libsdl.org/) is under the [Zlib license](https://github.com/libsdl-org/SDL/blob/main/LICENSE.txt)
- [ImGui](https://github.com/ocornut/imgui) is under the [MIT license](https://github.com/ocornut/imgui/blob/master/LICENSE.txt)
- [inih](https://github.com/benhoyt/inih) is under the [New BSD license](https://github.com/benhoyt/inih/blob/master/LICENSE.txt)
- [codeslinger](http://www.codeslinger.co.uk/pages/projects/chip8.html) was a precious resources for me and especially to better understand how CHIP-8 instructions work.
- David Winter's [CHIP-8 emulator](https://www.pong-story.com/chip8/)