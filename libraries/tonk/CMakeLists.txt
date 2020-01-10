cmake_minimum_required(VERSION 3.5)
project(tonk)

set(LIB_INCLUDE_FILES
    include/tonk.h
    include/tonk_file_transfer.h
    include/TonkCppSDK.hpp
)

set(LIB_SOURCE_FILES
    ${LIB_INCLUDE_FILES}
    src/tonk.cpp
    src/tonk_file_transfer.cpp
    src/TonkCppSDK.cpp
    src/MappedFile.cpp
    src/MappedFile.h
    src/TonkineseBandwidth.cpp
    src/TonkineseBandwidth.h
    src/TonkineseConnection.cpp
    src/TonkineseConnection.h
    src/TonkineseEncryption.cpp
    src/TonkineseEncryption.h
    src/StrikeRegister.cpp
    src/StrikeRegister.h
    src/PacketCompression.cpp
    src/PacketCompression.h
    src/TonkineseFirewall.cpp
    src/TonkineseFirewall.h
    src/TonkineseFlood.cpp
    src/TonkineseFlood.h
    src/TonkineseIncoming.cpp
    src/TonkineseIncoming.h
    src/TonkineseInterfaces.h
    src/TonkineseMaps.cpp
    src/TonkineseMaps.h
    src/TonkineseOutgoing.cpp
    src/TonkineseOutgoing.h
    src/TonkineseProtocol.cpp
    src/TonkineseProtocol.h
    src/TonkineseSession.cpp
    src/TonkineseSession.h
    src/TonkineseTools.cpp
    src/TonkineseTools.h
    src/TonkineseUDP.cpp
    src/TonkineseUDP.h
    src/TonkineseNAT.cpp
    src/TonkineseNAT.h
    src/WLANOptimizer.cpp
    src/WLANOptimizer.h
    src/TimeSync.cpp
    src/TimeSync.h
    src/gf256.cpp
    src/gf256.h
    src/siamese.cpp
    src/siamese.h
    src/SiameseCommon.cpp
    src/SiameseCommon.h
    src/SiameseDecoder.cpp
    src/SiameseDecoder.h
    src/SiameseEncoder.cpp
    src/SiameseEncoder.h
    src/SiameseSerializers.h
    src/SiameseTools.cpp
    src/SiameseTools.h
    src/Counter.h
    src/Logger.cpp
    src/Logger.h
    src/PacketAllocator.cpp
    src/PacketAllocator.h
    src/cymric.cpp
    src/cymric.h
    thirdparty/asio.hpp
    thirdparty/blake2.h
    thirdparty/blake2b-ref.c
    thirdparty/blake2-impl.h
    thirdparty/chacha.c
    thirdparty/chacha.h
    thirdparty/chacha_blocks_ref.c
    thirdparty/IncludeAsio.h
    thirdparty/t1ha.c
    thirdparty/t1ha.h
)

set(TONK_TESTS_SRC_FILES
    tests/TonkTestTools.cpp
    tests/TonkTestTools.h
    tests/BandwidthControlTest.cpp
    tests/BandwidthControlTest.h
    tests/TonkUnitTest.cpp
    tests/mau/mau.cpp
    tests/mau/mau.h
    tests/mau/MauProxy.cpp
    tests/mau/MauProxy.h
    tests/mau/MauTools.cpp
    tests/mau/MauTools.h
)

include_directories(thirdparty include .)

add_library(tonk STATIC
    ${LIB_SOURCE_FILES}
    ${ZSTD_SOURCE_FILES}
)
target_include_directories(tonk PUBLIC include)
target_compile_definitions(tonk PRIVATE
    #LOGGER_DISABLE_ATEXIT=1
    SIAMESE_BUILDING=1
    TONK_BUILDING=1
    TONK_DLL=0
    TONK_DISABLE_SHIM=1
)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

target_link_libraries(tonk PUBLIC
    zstd
    Threads::Threads
)

include_directories(src)
add_executable(tonk_tests ${TONK_TESTS_SRC_FILES})
target_link_libraries(tonk_tests tonk)
