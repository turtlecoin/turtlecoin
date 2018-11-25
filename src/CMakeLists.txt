add_definitions(-DSTATICLIB)

include_directories(${CMAKE_SOURCE_DIR}/external/rocksdb/include)
include_directories(${CMAKE_SOURCE_DIR}/external/linenoise)
include_directories(${CMAKE_SOURCE_DIR}/external/json)
include_directories(${CMAKE_SOURCE_DIR}/external/cxxopts)
include_directories(${CMAKE_SOURCE_DIR}/external/cryptopp)

file(GLOB_RECURSE BlockchainExplorer BlockchainExplorer/*)
file(GLOB_RECURSE Common Common/*)
file(GLOB_RECURSE Crypto crypto/*)
file(GLOB_RECURSE CryptoNoteCore CryptoNoteCore/* CryptoNoteConfig.h)
file(GLOB_RECURSE CryptoNoteProtocol CryptoNoteProtocol/*)
file(GLOB_RECURSE TurtleCoind Daemon/*)
file(GLOB_RECURSE Http HTTP/*)
file(GLOB_RECURSE Logging Logging/*)
file(GLOB_RECURSE miner Miner/*)
file(GLOB_RECURSE Mnemonics Mnemonics/*)
file(GLOB_RECURSE NodeRpcProxy NodeRpcProxy/*)
file(GLOB_RECURSE P2p P2p/*)
file(GLOB_RECURSE Rpc Rpc/*)
file(GLOB_RECURSE Serialization Serialization/*)
file(GLOB_RECURSE Transfers Transfers/*)
file(GLOB_RECURSE Wallet Wallet/*)
file(GLOB_RECURSE WalletBackend WalletBackend/*)
file(GLOB_RECURSE JsonRpcServer JsonRpcServer/*)
file(GLOB_RECURSE service WalletService/*)
file(GLOB_RECURSE zedwallet zedwallet/*)
file(GLOB_RECURSE CryptoTest CryptoTest/*)
file(GLOB_RECURSE zedwallet++ zedwallet++/*)

if(MSVC)
file(GLOB_RECURSE System System/* Platform/Windows/System/*)
elseif(APPLE)
file(GLOB_RECURSE System System/* Platform/OSX/System/* Platform/Posix/System/*)
else()
file(GLOB_RECURSE System System/* Platform/Linux/System/* Platform/Posix/System/*)
endif()

# This appears to be an IDE thing, to group files together.
# https://cmake.org/cmake/help/v3.0/command/source_group.html
# Probably not what you need to be looking at if something isn't building
source_group("" FILES $${Common} ${Crypto} ${CryptoNoteCore} ${CryptoNoteProtocol} ${TurtleCoind} ${JsonRpcServer} ${Http} ${Logging} ${miner} ${Mnemonics} ${NodeRpcProxy} ${P2p} ${Rpc} ${Serialization} ${System} ${Transfers} ${Wallet} ${WalletBackend} ${zedwallet} ${zedwallet++} ${CryptoTest})

add_library(BlockchainExplorer ${BlockchainExplorer})
add_library(Common ${Common})
add_library(Crypto ${Crypto})
add_library(CryptoNoteCore ${CryptoNoteCore})
add_library(Http ${Http})
add_library(Logging ${Logging})
add_library(Mnemonics ${Mnemonics})
add_library(NodeRpcProxy ${NodeRpcProxy})
add_library(Rpc ${Rpc})
add_library(P2P ${CryptoNoteProtocol} ${P2p})
add_library(Serialization ${Serialization})
add_library(System ${System})
add_library(Transfers ${Transfers})
add_library(Wallet ${Wallet})
add_library(WalletBackend ${WalletBackend})
add_library(JsonRpcServer ${JsonRpcServer})
add_library(WalletService ${service})

if(MSVC)
  set(DAEMON_SOURCES_OS
    BinaryInfo/daemon.rc
  )
  set(ZED_WALLET_SOURCES_OS
    BinaryInfo/zedwallet.rc
  )
  set(MINER_SOURCES_OS
    BinaryInfo/miner.rc
  )
  set(PG_SOURCES_OS
    BinaryInfo/service.rc
  )
  set(CT_SOURCES_OS
    BinaryInfo/cryptotest.rc
  )
endif()

add_executable(TurtleCoind ${TurtleCoind} ${DAEMON_SOURCES_OS})
add_executable(zedwallet ${zedwallet} ${ZED_WALLET_SOURCES_OS})
add_executable(zedwallet++ ${zedwallet++} ${ZED_WALLET_SOURCES_OS})
add_executable(service ${service} ${PG_SOURCES_OS})
add_executable(miner ${miner} ${MINER_SOURCES_OS})
add_executable(cryptotest ${CryptoTest} ${CT_SOURCES_OS})

if(MSVC)
  target_link_libraries(System ws2_32)
  target_link_libraries(TurtleCoind Rpcrt4)
  target_link_libraries(service Rpcrt4)
endif ()

# A bit of hackery so we don't have to do the if/else/ for every target that
# wants to use filesystem
add_library(__filesystem INTERFACE)

# Windows works out of the box
if (APPLE)
    target_link_libraries(__filesystem INTERFACE /usr/local/opt/llvm/lib/libc++fs.a)
elseif (UNIX)
    target_link_libraries(__filesystem INTERFACE stdc++fs)
endif()

# Add the dependencies we need
target_link_libraries(Common __filesystem)
target_link_libraries(CryptoNoteCore Common Logging Crypto P2P Rpc Http Serialization System ${Boost_LIBRARIES})
target_link_libraries(CryptoNoteCore Common Crypto Logging Serialization)
target_link_libraries(P2P CryptoNoteCore Logging ${Boost_LIBRARIES} upnpc-static)
target_link_libraries(Rpc CryptoNoteCore Logging P2P)
target_link_libraries(WalletService Mnemonics)

if(MSVC)
	target_link_libraries(TurtleCoind P2P Rpc Serialization System Http Logging CryptoNoteCore Crypto Common rocksdb ${Boost_LIBRARIES} )
else()
	target_link_libraries(TurtleCoind P2P Rpc Serialization System Http Logging CryptoNoteCore Crypto Common rocksdblib ${Boost_LIBRARIES} )
endif()

target_link_libraries(zedwallet Mnemonics Wallet NodeRpcProxy Transfers Rpc Http CryptoNoteCore System Logging Common WalletBackend)
target_link_libraries(service WalletService JsonRpcServer Wallet NodeRpcProxy Transfers CryptoNoteCore Crypto Rpc Http System Logging Common WalletBackend ${Boost_LIBRARIES})
target_link_libraries(Wallet NodeRpcProxy Transfers Rpc P2P upnpc-static Http Serialization CryptoNoteCore System Logging Common Crypto ${Boost_LIBRARIES})
target_link_libraries(miner CryptoNoteCore Rpc Serialization System Http Logging Common Crypto ${Boost_LIBRARIES})
target_link_libraries(cryptotest Crypto Common)

target_link_libraries(WalletBackend Mnemonics Wallet NodeRpcProxy Transfers Rpc Http CryptoNoteCore System Logging Common ${Boost_LIBRARIES} cryptopp-static __filesystem)
target_link_libraries(zedwallet++ WalletBackend)

# Add dependencies means we have to build the latter before we build the former
# In this case it's because we need to have the current version name rather
# than a cached one
add_dependencies(WalletService version)
add_dependencies(JsonRpcServer version)
add_dependencies(miner version)
add_dependencies(Rpc version)
add_dependencies(TurtleCoind version)
add_dependencies(zedwallet version)
add_dependencies(service version)
add_dependencies(P2P version)
add_dependencies(cryptotest version)

# Finally build the binaries
set_property(TARGET TurtleCoind PROPERTY OUTPUT_NAME "nibbled")
set_property(TARGET zedwallet PROPERTY OUTPUT_NAME "NBXwallet")
set_property(TARGET zedwallet++ PROPERTY OUTPUT_NAME "NBXwallet-beta")
set_property(TARGET service PROPERTY OUTPUT_NAME "nibble-service")
set_property(TARGET miner PROPERTY OUTPUT_NAME "nibbler")
set_property(TARGET cryptotest PROPERTY OUTPUT_NAME "cryptotest")

# Additional make targets
add_custom_target(pool DEPENDS TurtleCoind service)
add_custom_target(solominer DEPENDS TurtleCoind zedwallet miner)
add_custom_target(cli DEPENDS TurtleCoind zedwallet)
