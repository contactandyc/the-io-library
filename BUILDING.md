# BUILDING

This project: **The IO Library**
Version: **0.0.1**

## Local build

```bash
# one-shot build + install
./build.sh install
```

Or run the steps manually:

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j"$(nproc || sysctl -n hw.ncpu || echo 4)"
sudo cmake --install .
```



## Install dependencies (from `deps.libraries`)


### System packages (required)

```bash
sudo apt-get update && sudo apt-get install -y zlib1g-dev
```



### Development tooling (optional)

```bash
sudo apt-get update && sudo apt-get install -y valgrind gdb perl autoconf automake libtool
```



### a-memory-library

Clone & build:

```bash
git clone --depth 1 --single-branch "https://github.com/contactandyc/a-memory-library.git" "a-memory-library"
cd "a-memory-library"
./build.sh clean
./build.sh install
cd ..
rm -rf "a-memory-library"
```


### the-lz4-library

Clone & build:

```bash
git clone --depth 1 --single-branch "https://github.com/contactandyc/the-lz4-library.git" "the-lz4-library"
cd "the-lz4-library"
./build.sh clean
./build.sh install
cd ..
rm -rf "the-lz4-library"
```


### the-macro-library

Clone & build:

```bash
git clone --depth 1 --single-branch "https://github.com/contactandyc/the-macro-library.git" "the-macro-library"
cd "the-macro-library"
./build.sh clean
./build.sh install
cd ..
rm -rf "the-macro-library"
```


### ZLIB

Install via package manager:

```bash
sudo apt-get update && sudo apt-get install -y zlib1g-dev
```

