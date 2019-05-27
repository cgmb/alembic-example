# Alembic Example

This is a minimal example of an Alembic mesh animation compatible with Blender.
Each frame is an independent mesh that may not share anything at all with the
previous frame.

## Ubuntu Dependencies

    sudo apt install cmake build-essential ninja-build \
        libopenexr-dev extra-cmake-modules

## How to Build

    cmake -H. -Bbuild -G Ninja && cmake --build build

## How to Run

    build/alex frames/*

## Working with Alembic files

The output file is `out.abc`. The example animation in `frames/` is very
low-resolution, but still looks pretty good when filtered and rendered in
Blender. Note that the version of Blender distributed by apt on Ubuntu 18.04
does not support Alembic. Use the official binary release from [blender.org][3]
to import Alembic files.

![Rendered example animation of a droplet spreading.][4]

## License

The Alembic library itself is governed by the [Alembic license][1]. All other
code in this repository was authored and published by Cordell Bloor in 2019
under the [CC0][2] license.

[1]: 3rdparty/alembic/LICENSE.txt
[2]: LICENSE
[3]: https://www.blender.org/
[4]: alex.gif
