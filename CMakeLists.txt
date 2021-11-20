cmake_minimum_required(VERSION 3.20)
project(CAN_BCM_Boost_Asio)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-lboost_system -lboost_thread -pthread")

include_directories(include)
add_executable(CAN_BCM_Boost_Asio src/main.cpp src/CANConnector.cpp src/InterfaceIndexIO.cpp)
