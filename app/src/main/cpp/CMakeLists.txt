cmake_minimum_required(VERSION 3.22.1)

project("myrtmp")

add_subdirectory(librtmp)
add_subdirectory(libbreakpad)

include_directories(
        librtmp
        libx264/include
        libfaac/include
        libbreakpad/src
        libbreakpad/src/common/android/include
)

link_directories(
        ${CMAKE_SOURCE_DIR}/libx264/libs/${CMAKE_ANDROID_ARCH_ABI}
        ${CMAKE_SOURCE_DIR}/libfaac/libs/${CMAKE_ANDROID_ARCH_ABI}
)

add_library(
        ${CMAKE_PROJECT_NAME}
        SHARED
        native-lib.cpp
        VideoChannel.cpp
        AudioChannel.cpp
)

target_link_libraries(
        ${CMAKE_PROJECT_NAME}
        android
        log
        rtmp
        x264
        faac
        breakpad
)