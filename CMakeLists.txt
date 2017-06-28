cmake_minimum_required(VERSION 3.6)
project(ffmpeg-study)

set(CMAKE_CXX_STANDARD 11)
aux_source_directory(. SOURCE_FILES)
#set(SOURCE_FILES main.c yuv2jpg.c yuv2h64.c)

#循环遍历
foreach(path ${SOURCE_FILES})
    message(${path})
endforeach(path)
#排除某个文件
list(REMOVE_ITEM SOURCE_FILES ./muxing.c ./demuxing_decoding.c)
list(REMOVE_ITEM SOURCE_FILES ./filtering_video.c ./transcoding.c)

#网上找的一个例子
list(REMOVE_ITEM SOURCE_FILES ./SplitVideo.cpp ./Test.cpp)

add_executable(ffmpeg ${SOURCE_FILES})

include_directories(
        E:/ffmpeg/ffmpeg-20161005-54220ce-win32-dev/include
)
link_directories(
        E:/ffmpeg/ffmpeg-20161005-54220ce-win32-dev/lib
)
add_executable(ffmpeg-study ${SOURCE_FILES})
target_link_libraries(
        ffmpeg-study

        avformat
        avcodec
        avdevice
        avfilter
        avutil
        postproc
        swresample
        swscale
)

