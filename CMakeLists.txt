cmake_minimum_required(VERSION 3.30)
project(MyRedis)

set(CMAKE_CXX_STANDARD 20)

add_executable(MyRedis main.cpp
        WindowsSockets.h
        RedisSocket.h
        RedisServer.h
        ServerAddress.h
        RedisClient.h
        ClientAddress.h
        Address.h
        Utils.h
        Poller.h
)
target_link_libraries(MyRedis ws2_32)