add_definitions(-DSTATICLIB)

include_directories(${CMAKE_SOURCE_DIR}/external/rocksdb/include)

file(GLOB_RECURSE BlockchainExplorer BlockchainExplorer/*)
file(GLOB_RECURSE Common Common/*)
file(GLOB_RECURSE ConnectivityTool ConnectivityTool/*)
file(GLOB_RECURSE Crypto crypto/*)
file(GLOB_RECURSE CryptoNoteCore CryptoNoteCore/* CryptoNoteConfig.h)
file(GLOB_RECURSE CryptoNoteProtocol CryptoNoteProtocol/*)
file(GLOB_RECURSE TurtleCoind Daemon/*)
file(GLOB_RECURSE Http HTTP/*)
file(GLOB_RECURSE InProcessNode InProcessNode/*)
file(GLOB_RECURSE Logging Logging/*)
file(GLOB_RECURSE Miner Miner/*)
file(GLOB_RECURSE Mnemonics Mnemonics/*)
file(GLOB_RECURSE NodeRpcProxy NodeRpcProxy/*)
file(GLOB_RECURSE P2p P2p/*)
file(GLOB_RECURSE Rpc Rpc/*)
file(GLOB_RECURSE Serialization Serialization/*)
file(GLOB_RECURSE Transfers Transfers/*)
file(GLOB_RECURSE Wallet Wallet/*)
file(GLOB_RECURSE WalletLegacy WalletLegacy/*)
file(GLOB_RECURSE JsonRpcServer JsonRpcServer/*)
file(GLOB_RECURSE PaymentGate PaymentGate/*)
file(GLOB_RECURSE PaymentGateService PaymentGateService/*)
file(GLOB_RECURSE PoolWallet PoolWallet/*)
file(GLOB_RECURSE zedwallet zedwallet/*)

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
source_group("" FILES $${Common} ${ConnectivityTool} ${Crypto} ${CryptoNoteCore} ${CryptoNoteProtocol} ${TurtleCoind} ${JsonRpcServer} ${Http} ${Logging} ${Miner} ${Mnemonics} ${NodeRpcProxy} ${PoolWallet} ${P2p} ${Rpc} ${Serialization} ${System} ${Transfers} ${Wallet} ${WalletLegacy} ${zedwallet})

add_library(BlockchainExplorer ${BlockchainExplorer})
add_library(Common ${Common})
add_library(Crypto ${Crypto})
add_library(CryptoNoteCore ${CryptoNoteCore})
add_library(Http ${Http})
add_library(InProcessNode ${InProcessNode})
add_library(Logging ${Logging})
add_library(Mnemonics ${Mnemonics})
add_library(NodeRpcProxy ${NodeRpcProxy})
add_library(Rpc ${Rpc})
add_library(P2P ${CryptoNoteProtocol} ${P2p})
add_library(Serialization ${Serialization})
add_library(System ${System})
add_library(Transfers ${Transfers})
add_library(Wallet ${Wallet} ${WalletLegacy})
add_library(PaymentGate ${PaymentGate})
add_library(JsonRpcServer ${JsonRpcServer})

if(MSVC)
  set(DAEMON_SOURCES_OS
    BinaryInfo/daemon.rc
  )
  set(ZED_WALLET_SOURCES_OS
    BinaryInfo/zedwallet.rc
  )
  set(POOLWALLET_SOURCES_OS
    BinaryInfo/poolwallet.rc
  )
  set(MINER_SOURCES_OS
    BinaryInfo/miner.rc
  )
  set(PG_SOURCES_OS
    BinaryInfo/walletd.rc
  )
endif()

add_executable(TurtleCoind ${TurtleCoind} ${DAEMON_SOURCES_OS})
add_executable(zedwallet ${zedwallet} ${ZED_WALLET_SOURCES_OS})
add_executable(PaymentGateService ${PaymentGateService} ${PG_SOURCES_OS})
add_executable(PoolWallet ${PoolWallet} ${POOLWALLET_SOURCES_OS})
add_executable(Miner ${Miner} ${MINER_SOURCES_OS})

if(MSVC)
  target_link_libraries(System ws2_32)
  target_link_libraries(TurtleCoind Rpcrt4)
  target_link_libraries(PaymentGateService Rpcrt4)
endif ()

# Add the dependencies we need
target_link_libraries(CryptoNoteCore Common Logging Crypto P2P Rpc Http Serialization System ${Boost_LIBRARIES})
target_link_libraries(CryptoNoteCore Common Crypto Logging Serialization)
target_link_libraries(P2P CryptoNoteCore Logging ${Boost_LIBRARIES} upnpc-static)
target_link_libraries(Rpc CryptoNoteCore Logging P2P)
if(MSVC)
	target_link_libraries(TurtleCoind P2P Rpc Serialization System Http Logging CryptoNoteCore Crypto Common rocksdb ${Boost_LIBRARIES} )
else()
	target_link_libraries(TurtleCoind P2P Rpc Serialization System Http Logging CryptoNoteCore Crypto Common rocksdblib ${Boost_LIBRARIES} )
endif()
target_link_libraries(zedwallet Mnemonics Wallet NodeRpcProxy Transfers Rpc Http CryptoNoteCore System Logging Common ${Boost_LIBRARIES})
target_link_libraries(PaymentGate Mnemonics)
if(MSVC)
	target_link_libraries(PaymentGateService PaymentGate JsonRpcServer Wallet NodeRpcProxy Transfers CryptoNoteCore Crypto Rpc Http System Logging Common InProcessNode rocksdb ${Boost_LIBRARIES})
else()
	target_link_libraries(PaymentGateService PaymentGate JsonRpcServer Wallet NodeRpcProxy Transfers CryptoNoteCore Crypto Rpc Http System Logging Common InProcessNode rocksdblib ${Boost_LIBRARIES})
endif()
target_link_libraries(PoolWallet Wallet NodeRpcProxy Transfers Rpc P2P upnpc-static Http Serialization CryptoNoteCore System Logging Common Crypto ${Boost_LIBRARIES})
target_link_libraries(Miner CryptoNoteCore Rpc Serialization System Http Logging Common Crypto ${Boost_LIBRARIES})

# Add dependencies means we have to build the latter before we build the former
# In this case it's because we need to have the current version name rather
# than a cached one
add_dependencies(Rpc version)
add_dependencies(TurtleCoind version)
add_dependencies(zedwallet version)
add_dependencies(PaymentGateService version)
add_dependencies(PoolWallet version)
add_dependencies(P2P version)

# Finally build the binaries
set_property(TARGET TurtleCoind PROPERTY OUTPUT_NAME "TurtleCoind")
set_property(TARGET zedwallet PROPERTY OUTPUT_NAME "zedwallet")
set_property(TARGET PaymentGateService PROPERTY OUTPUT_NAME "walletd")
set_property(TARGET PoolWallet PROPERTY OUTPUT_NAME "poolwallet")
set_property(TARGET Miner PROPERTY OUTPUT_NAME "miner")
