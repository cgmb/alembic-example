# Alembic Example

This is a minimal example of an Alembic mesh animation compatable with Blender.
Each frame is an independent mesh that may not share anything at all with the
previous frame.

## Ubuntu Dependencies

    sudo apt install cmake build-essential ninja-build \
        libopenexr-dev extra-cmake-modules

## How to Build

    cmake -H. -Bbuild -G Ninja && cmake --build build

## How to Run

    build/alex frames/*

## License

The Alembic library itself is governed by the [Alembic license][1]. All other
code in this repository was authored and published by Cordell Bloor in 2019
under the [CC0][2] license.

[1]: 3rdparty/alembic/LICENSE.txt
[2]: LICENSE
