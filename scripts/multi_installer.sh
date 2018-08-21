#!/usr/bin/env bash

# Turtlecoin Multi-installer
# a one line clone-and-compile for turtlecoin:
#
#     ` $ curl -sL "https://raw.githubusercontent.com/turtlecoin/turtlecoin/master/scripts/multi_installer.sh" | bash
#
# Supports Ubuntu 16.04 LTS, OSX 10.10+
# Supports building project from current directory (automatic detection)

set -o errexit
set -o pipefail

_colorize() {
  local code="\033["
  case "$1" in
    red    |  r) color="${code}1;31m";;
    green  |  g) color="${code}1;32m";;
    purple |  p) color="${code}1;35m";;
    *) local text="$1"
  esac
  [ -z "$text" ] && local text="$color$2${code}0m"
  printf "$text"
}

_note() {
    local msg=`echo $1`
    _colorize purple "$msg" && echo
}

_fail() {
    local msg=`echo \'$1\'`
    _colorize red "Failure: $msg" | tee -a build.log && echo
    _colorize red "Please check build.log and if you need help check out the team discord @ 'https://discordapp.com/invite/NZ7QYJA'" && echo
    _colorize purple "Exiting script" && echo
    exit 1
}

_set_wd() {
    if [ -d "$PWD/.git" ] && [ -f "$PWD/Makefile" ]; then
        _note "Building project from current working directory ($PWD)"
    else
        _note "Cloning project with git..."
        if [ -d "$PWD"/turtlecoin ]; then
            read -r -p "${1:-turtlecoin directory already exists. Overwrite? [y/N]} " response
            case "$response" in
                [yY][eE][sS|[yY])
                    _colorize red "Overwriting old turtlecoin directory" && echo
                    rm -rf "$PWD"/turtlecoin
                    ;;
                *)
                    _fail "turtlecoin directory already exists. Aborting..."
                    ;;
            esac
        fi
        mkdir turtlecoin
        git clone -b master -q https://github.com/turtlecoin/turtlecoin turtlecoin   >>build.log 2>&1 || _fail "Unable to clone git repository. Please see build.log for more information"
        cd turtlecoin
    fi
}

_build_turtlecoin() {
    _note "Building turtlecoin from source (this might take a while)..."
    if [ -d build ]; then
        _colorize red "Overwriting old build directory" && echo
        rm -rf build
    fi

    local _threads=`python -c 'import multiprocessing as mp; print(mp.cpu_count())'`
    echo "Using ${_threads} threads"

    mkdir build && cd $_
    cmake --silent -DDO_TESTS=OFF .. >>build.log 2>&1 || _fail "Unable to run cmake. Please see build.log for more information"
    make --silent -j"$_threads"  >>build.log 2>&1 || _fail "Unable to run make. Please see build.log for more information"
    _note "Compilation completed!"
}

_configure_ubuntu() {
    [ "`lsb_release -r -s | cut -d'.' -f1 `" -ge 16 ] || _fail "Your Ubuntu version `lsb_release -r -s` is below the requirements to run this installer. Please consider upgrading to a later release"
    _note "The installer will now update your package manager and install required packages (this might take a while)..."
    _sudo=""
    if (( $EUID != 0 )); then
        _sudo="sudo"
        _note "Sudo privileges required for package installation"
    fi
    $_sudo apt-get update -qq
    $_sudo apt-get install -qq -y git build-essential python-dev gcc g++ git cmake libboost-all-dev >>build.log 2>&1 || _fail "Unable to install build dependencies. Please see build.log for more information"

    export CXXFLAGS="-std=gnu++11"
}

_configure_debian() {
    [ "`lsb_release -r -s | cut -d'.' -f1 `" -ge 9 ] || _fail "Your Debian GNU/Linux version `lsb_release -r -s` is below the requirements to run this installer. Please consider upgrading to a later release"
    _note "The installer will now update your package manager and install required packages (this might take a while)..."
    _sudo=""
    if (( $EUID != 0 )); then
        _sudo="sudo"
        _note "Sudo privileges required for package installation"
    fi
    $_sudo apt-get update -qq
    $_sudo apt-get install -qq -y git build-essential python-dev gcc g++ git cmake libboost-all-dev librocksdb-dev  >>build.log 2>&1 || _fail "Unable to install build dependencies. Please see build.log for more information"

    export CXXFLAGS="-std=gnu++11"
}

_configure_linux() {
    if [ "$(awk -F= '/^NAME/{print $2}' /etc/os-release)" = "\"Ubuntu\"" ]; then
        _configure_ubuntu
    elif [ "$(awk -F= '/^NAME/{print $2}' /etc/os-release)" = "\"Debian GNU/Linux\"" ]; then
        _configure_debian
    else
        _fail "Your OS version isn't supported by this installer. Please consider adding support for your OS to the project ('https://github.com/turtlecoin')"
    fi
}

_configure_osx() {
    [ ! "`echo ${OSTYPE:6} | cut -d'.' -f1`" -ge 14 ] && _fail "Your OS X version ${OSTYPE:6} is below the requirements to run this installer. Please consider upgrading to the latest release";
    if [[ $(command -v brew) == "" ]]; then
        _note "Homebrew package manager was not found using \`command -v brew\`"
        _note "Installing Xcode if missing, setup will resume after completion..."
        xcode-select --install
        _note "Running the installer for homebrew, setup will resume after completion..."
        /usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
    fi
    _note "Updating homebrew and installing software dependencies..."
    brew update --quiet
    brew install --quiet git cmake boost rocksdb
}

_configure_os() {
    _note "Configuring your operating system and installing required software..."
    _unameOut="$(uname -s)"
    case "${_unameOut}" in
        Linux*)
            _configure_linux
            ;;
        Darwin*)
            _configure_osx
            ;;
        *)
            _fail "This installer only runs on OSX 10.10+ and Ubuntu 16.04+. Please consider adding support for your OS to the project ('https://github.com/turtlecoin')"
            ;;
    esac
    _note "Operating system configuration completed. You're halfway there!"
}

_note "Turtlecoin Multi_Installer v1.0 (pepperoni)"
_colorize green " _______         _   _       _____      _       \n|__   __|       | | | |     / ____|    (_)      \n   | |_   _ _ __| |_| | ___| |     ___  _ _ __  \n   | | | | | '__| __| |/ _ \ |    / _ \| | '_ \ \n   | | |_| | |  | |_| |  __/ |___| (_) | | | | |\n   |_|\__,_|_|   \__|_|\___|\_____\___/|_|_| |_|\n" && echo

_configure_os

_set_wd
_build_turtlecoin

_note "Installation complete!"
_note "Look in 'turtlecoin/build/src/' for the executible binaries. See 'https://github.com/turtlecoin/turtlecoin' for more project support. Cowabunga!"
