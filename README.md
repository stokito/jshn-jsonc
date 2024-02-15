# jshn: shell library to work with JSON

jshn (JSON SHell Notation), a small utility and shell library for parsing and generating JSON data.
See [documentation](https://openwrt.org/docs/guide-developer/jshn) for examples.
It works with BusyBox ash, bash, zsh but may NOT work with dash or simplest POSIX shells.

## Installation

### Ubuntu
The plugin can be installed in Ubuntu via [PPA](https://code.launchpad.net/~stokito/+archive/ubuntu/utils):

    sudo add-apt-repository ppa:stokito/utils
    sudo apt-get update
    sudo apt install jshn-jsonc

### From sources
To build and install from sources:

    sudo apt install cmake pkg-config libjson-c-dev
    git clone https://github.com/stokito/jshn-jsonc.git
    cd jshn-jsonc
    mkdir build
    cd build
    cmake ..
    make
    sudo make install

## Fork
This is a fork of OpenWrt jshn library that doesn't have a dependency to [libubox](https://gitlab.com/openwrt/project/libubox).
Instead it uses internal functions from `linkhash.c` of [json-c](https://github.com/json-c/json-c) itself.
The internal functions aren't part of public API so it's not guaranteed that after json-c update it won't be broken.
But this is a very stable functions so this is probably never happen.

Another difference is that `-i` and `-n` args are ignored i.e. no pretty print since it's not used inside of shell script.

While there is no dependency to `libubox` the path to include library still has `libubox`:

    /usr/share/libubox/jshn.sh

This is just to be compatible with existing scripts.

## License
ISC

## See also
* [Awesome JSON](https://github.com/burningtree/awesome-json) list of other usefull tools
