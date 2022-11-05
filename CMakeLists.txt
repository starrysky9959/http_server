cmake_minimum_required(VERSION 3.0.0)
project(lab1 VERSION 0.1.0)

include(CTest)
enable_testing()

add_executable(
    ${PROJECT_NAME} 
    src/main.cpp
    src/buffer/Buffer.cpp
    src/http/HttpRequest.cpp
    src/http/HttpResponse.cpp
    src/http/HttpConnection.cpp
    src/log/Log.cpp
    src/server/Epoller.cpp
    src/server/WebServer.cpp
    )
target_include_directories(
    ${PROJECT_NAME} PUBLIC
    src/)
target_link_libraries(
    ${PROJECT_NAME} 
    -pthread
    -lssl -lcrypto
    )

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
