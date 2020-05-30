### Compiling TurtleCoin

##### Table of Contents

-   **Linux**
    -   [Linux Dependencies](#linux-dependencies)
    -   [Ubuntu with GCC](#ubuntu-with-gcc)
    -   [Ubuntu with CLANG](#ubuntu-with-clang)
    -   [CentOS with GCC](#centos-with-gcc)
    -   [Generic Linux](#generic-linux)
-   **MacOS**
    -   [MacOS Dependencies](#macos-dependencies)
    -   [MacOS with CLANG](#macos-with-clang)
-   **Windows**
    -   [Windows Dependencies](#windows-dependencies)
    -   [Windows with VS2019](#windows-with-vs2019)

##### Build Optimization

The CMake build system will, by default, create optimized _native_ builds for your particular system type when you build the software. Using this method, the binaries created provide a better experience and all-together faster performance.

##### Making Portable Binaries

However, if you wish to create _portable_ binaries that can be shared between systems, specify `-DARCH=default` in your CMake arguments during the build process. Note that _portable_ binaries will have a noticable difference in performance than _native_ binaries. For this reason, it is always best to build for your particular system if possible.

## Linux

##### Linux Dependencies

**Note:** Individual names for these packages may differ depending on your Linux distribution.

-   [Boost](https://www.boost.org/)
-   [OpenSSL](https://www.openssl.org/)
-   [Cmake (3.8, higher)](https://cmake.org/download/)
-   [GNU Make](https://ftp.gnu.org/gnu/make/)
-   [Git](https://git-scm.com/)
-   [GCC 7, higher](https://gcc.gnu.org/) or [CLANG 6, higher](https://clang.llvm.org/)

##### Ubuntu with GCC

-   `sudo add-apt-repository ppa:ubuntu-toolchain-r/test -y`
-   `sudo apt-get update`
-   `sudo apt-get install aptitude -y`
-   `sudo aptitude install -y build-essential g++-8 gcc-8 git libboost-all-dev python-pip libssl-dev`
-   `sudo pip install cmake`
-   `export CC=gcc-8`
-   `export CXX=g++-8`
-   `git clone -b master --single-branch https://github.com/turtlecoin/turtlecoin`
-   `cd turtlecoin`
-   `mkdir build`
-   `cd build`
-   `cmake ..`
-   `make`

The binaries will be in the `src` folder when you are complete.

-   `cd src`
-   `./TurtleCoind --version`

##### Ubuntu with CLANG

###### For Ubuntu 16.04 (Xenial) users:

-   `sudo add-apt-repository "deb https://apt.llvm.org/xenial/ llvm-toolchain-xenial 6.0 main"`

###### For Ubuntu 18.04 (Bionic) users:

-   `sudo add-apt-repository "deb https://apt.llvm.org/bionic/ llvm-toolchain-bionic 6.0 main"`

###### For Everyone:

-   `sudo add-apt-repository ppa:ubuntu-toolchain-r/test -y`
-   `wget -O - https://apt.llvm.org/Developer Tools.llvm-snapshot.gpg.key | sudo apt-key add -`

*   `sudo apt-get update`
*   `sudo apt-get install aptitude -y`
*   `sudo aptitude install -y -o Aptitude::ProblemResolver::SolutionCost='100*canceled-actions,200*removals' build-essential clang-6.0 libstdc++-7-dev git libboost-all-dev python-pip libssl-dev`
*   `sudo pip install cmake`
*   `export CC=clang-6.0`
*   `export CXX=clang++-6.0`
*   `git clone -b master --single-branch https://github.com/turtlecoin/turtlecoin`
*   `cd turtlecoin`
*   `mkdir build`
*   `cd build`
*   `cmake ..`
*   `make`

The binaries will be in the `src` folder when you are complete.

-   `cd src`
-   `./TurtleCoind --version`

##### CentOS with GCC

_CentOS compile instructions provided by @brandonlehmann_

-   `sudo yum update -y`
-   `sudo yum install -y epel-release centos-release-scl`
-   `sudo yum install -y devtoolset-8 cmake cmake3 wget git openssl-devel`
-   `sudo scl enable devtoolset-8 bash`
-   `wget https://dl.bintray.com/boostorg/release/1.68.0/source/boost_1_68_0.tar.gz`
-   `tar xzvf boost_1_68_0.tar.gz && cd boost_1_68_0`
-   `./bootstrap.sh --prefix=/usr/local/`
-   `./b2 -j$(nproc) -d0 install --with-system --with-filesystem --with-thread --with-date_time --with-chrono --with-regex --with-serialization --with-program_options`
-   `cd ..`
-   `git clone https://github.com/turtlecoin/turtlecoin/`
-   `mkdir -p turtlecoin/build`
-   `cd turtlecoin/build`
-   `cmake3 ..`
-   `make`

The binaries will be in the `src` folder when you are complete.

-   `cd src`
-   `./TurtleCoind --version`

##### Generic Linux

**Note:** If you want to use clang, ensure you set the environment variables `CC` and `CXX`.
See the ubuntu instructions for an example.

-   `git clone -b master --single-branch https://github.com/turtlecoin/turtlecoin`
-   `cd turtlecoin`
-   `mkdir build`
-   `cd build`
-   `cmake ..`
-   `make`

The binaries will be in the `src` folder when you are complete.

-   `cd src`
-   `./TurtleCoind --version`

## MacOS

##### MacOS Dependencies

-   [XCode](https://developer.apple.com/xcode/)
-   [CLANG](https://clang.llvm.org/)
-   [Brew](https://brew.sh/)
-   [Cmake (3.8, higher)](https://cmake.org/download/)
-   [GNU Make](https://ftp.gnu.org/gnu/make/)
-   [Git](https://git-scm.com/)

##### MacOS with CLANG

-   `which brew || /usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"`
-   `brew install --force cmake boost llvm@8 openssl`
-   `brew link --force llvm@8`
-   `ln -s /usr/local/opt/llvm@8 /usr/local/opt/llvm`
-   `export CC=/usr/local/opt/llvm@8/bin/clang`
-   `export CXX=/usr/local/opt/llvm@8/bin/clang++`
-   `git clone -b master --single-branch https://github.com/turtlecoin/turtlecoin`
-   `cd turtlecoin`
-   `mkdir build`
-   `cd build`
-   `cmake ..`
-   `make`

The binaries will be in the `src` folder when you are complete.

-   `cd src`
-   `./TurtleCoind --version`

## Windows

##### Windows Dependencies

You can build for 32-bit or 64-bit Windows. **If you're not sure, pick 64-bit.**

-   Download the [Build Tools for Visual Studio 2019](https://visualstudio.microsoft.com/thank-you-downloading-visual-studio/?sku=BuildTools&rel=16) Installer
-   When it opens up select **C++ build tools**, it automatically selects the needed parts. Make sure **MSVC v141 Build Tools** is selected.
-   Install Boost. Select the appropriate version for your system:
    -   [Boost 64-bit](https://bintray.com/boostorg/release/download_file?file_path=1.69.0%2Fbinaries%2Fboost_1_69_0-msvc-14.1-64.exe)
    -   [Boost 32-bit](https://bintray.com/boostorg/release/download_file?file_path=1.69.0%2Fbinaries%2Fboost_1_69_0-msvc-14.1-32.exe)
-   Install the latest full LTS version of OpenSSL (currently OpenSSL 1.1.1f). Select the appropriate version for your system:
    -   [OpenSSL 64-bit](https://slproweb.com/download/Win64OpenSSL-1_1_1g.exe)
    -   [OpenSSL 32-bit](https://slproweb.com/download/Win32OpenSSL-1_1_1g.exe)

##### Windows with VS2019

For 64-bit:

-   From the start menu, open **x64 Native Tools Command Prompt for VS 2019**.
-   `cd <your_turtlecoin_directory>`
-   `mkdir build`
-   `cd build`
-   `cmake -G "Visual Studio 16 2019" -A x64 .. -DBOOST_ROOT=C:/local/boost_1_69_0`
-   `MSBuild TurtleCoin.sln /p:Configuration=Release /p:PlatformToolset=v141 /m` or `MSBuild src\cli.vcxproj /p:Configuration=Release /p:PlatformToolset=v141 /m`

For 32-bit:

-   From the start menu, open **x86 Native Tools Command Prompt for VS 2019**.
-   `cd <your_turtlecoin_directory>`
-   `mkdir build`
-   `cd build`
-   `cmake -G "Visual Studio 16 2019" -A Win32 .. -DBOOST_ROOT=C:/local/boost_1_69_0`
-   `MSBuild TurtleCoin.sln /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v141 /m`

The binaries will be in the `src/Release` folder when you are complete.

-   `cd src`
-   `cd Release`
-   `TurtleCoind.exe --version`

[^ Return To Top](#compiling-turtlecoin)
