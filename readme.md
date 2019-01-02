# Beam Equihash 150/5 OpenCL Miner

```
Copyright 2018 The Beam Team
Copyright 2018 Wilke Trei 
```


# Usage
## One Liner Examples
```
Linux: ./beamMiner --server <hostName>:<portNumer> --key <apiKey> --devices <deviceList>
Windows: ./beamMiner --server <hostName>:<portNumer> --key <apiKey> --devices <deviceList>
```

## Parameters
### --server 
Passes the address and port of the node the miner will mine on to the miner.
The server address can be an IP or any other valid server address.- For example when the node
is running on the same computer and listens on port 17000 then use --server localhost:17000

### --key
Pass a valid API key from "stratum.api.keys" to the miner. Required to authenticate the miner at the node

### --devices (Optional)
Selects the devices to mine on. If not specified the miner will run on all devices found on the system. 
For example to mine on GPUs 0,1 and 3 but not number 2 use --devices 0,1,3
To list all devices that are present in the system and get their order start the miner with --devices -2 .
Then all devices will be listed, but none selected for mining. The miner closes when no devices were 
selected for mining or all selected miner fail in the compatibility check.

# How to build
## Windows
1. Install Visual Studio >= 2017 with CMake support.
1. Download and install Boost prebuilt binaries https://sourceforge.net/projects/boost/files/boost-binaries/1.68.0/boost_1_68_0-msvc-14.1-64.exe, also add `BOOST_ROOT` to the _Environment Variables_.
1. Download and install OpenSSL prebuilt binaries https://slproweb.com/products/Win32OpenSSL.html (`Win64 OpenSSL v1.1.0h` for example).
1. Download and install CUDA Toolkit https://developer.nvidia.com/cuda-downloads
1. Add `.../boost_1_68_0/lib64-msvc-14.1` to the _System Path_.
1. Open project folder in Visual Studio, select your target (`Release-x64` for example, if you downloaded 64bit Boost and OpenSSL) and select `CMake -> Build All`.
1. Go to `CMake -> Cache -> Open Cache Folder -> beam-opencl-miner` (you'll find `beam-opencl-miner.exe`).

## Linux (Ubuntu 14.04)
1. Install `gcc7` `boost` `ssl` packages.
```
  sudo add-apt-repository ppa:ubuntu-toolchain-r/test
  sudo apt update
  sudo apt install g++-7 libboost-all-dev libssl-dev -y
```
1. Set it up so the symbolic links `gcc`, `g++` point to the newer version:
```
  sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-7 60 \
                           --slave /usr/bin/g++ g++ /usr/bin/g++-7 
  sudo update-alternatives --config gcc
  gcc --version
  g++ --version
```
1. Install OpenCL
```
  sudo apt install ocl-icd-* opencl-headers
```
1. Install latest CMake 
```
  wget "https://cmake.org/files/v3.12/cmake-3.12.0-Linux-x86_64.sh"
  sudo sh cmake-3.12.0-Linux-x86_64.sh --skip-license --prefix=/usr
```
1. Go to beam-opencl-miner project folder and call `cmake -DCMAKE_BUILD_TYPE=Release . && make -j4`.
1. You'll find beam-opencl-miner binary in `bin` folder.

## Mac
1. Install Brew Package Manager.
1. Installed necessary packages using `brew install openssl boost cmake` command.
1. Add `export OPENSSL_ROOT_DIR="/usr/local/opt/openssl"` to the _Environment Variables_.
1. Go to beam-opencl-miner project folder and call `cmake -DCMAKE_BUILD_TYPE=Release . && make -j4`.
1. You'll find beam-opencl-miner binary in `bin` folder.


# Build status
[![Build Status](https://travis-ci.org/BeamMW/opencl-miner.svg?branch=master)](https://travis-ci.org/BeamMW/opencl-miner)
