set(PROJECT_NAME N-Grams)

project(${PROJECT_NAME} C CXX)

set(CMAKE_CXX_STANDARD 17)

add_executable(${PROJECT_NAME} src/main.cpp src/main.h indexing/myqueue.hpp parser/parser.cpp parser/parser.h indexing/processing.cpp indexing/processing.h prediction/prediction.h prediction/prediction.cpp)

set (Boost_USE_STATIC_LIBS OFF) 
set (Boost_USE_MULTITHREAD ON)  

find_package(Boost 1.71.0 COMPONENTS program_options system REQUIRED COMPONENTS filesystem locale system)
target_include_directories(${PROJECT_NAME} PRIVATE ${Boost_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} Boost::program_options Boost::system Boost::locale)

find_package(TBB REQUIRED)
target_link_libraries(${PROJECT_NAME} TBB::tbb)

find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} Threads::Threads)

if (APPLE)
    set(LibArchive_INCLUDE_DIR "/usr/local/opt/libarchive/include")
endif()

find_package(LibArchive REQUIRED)
target_include_directories(${PROJECT_NAME} PRIVATE ${LibArchive_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} LibArchive::LibArchive)

INSTALL(PROGRAMS $<TARGET_FILE:${PROJECT_NAME}> DESTINATION bin)

set(ALL_TARGETS ${PROJECT_NAME})

