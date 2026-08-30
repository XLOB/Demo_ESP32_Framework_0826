cmake_minimum_required(VERSION 3.16)

# set(EXTRA_COMPONENT_DIRS "main/components")
# 告诉 ESP-IDF 在 main/components 和 main/framework 下查找组件
set(EXTRA_COMPONENT_DIRS main/components main/framework)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(Demo_ESP32_Framework_0826)