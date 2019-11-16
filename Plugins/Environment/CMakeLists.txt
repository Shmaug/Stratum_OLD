cmake_minimum_required (VERSION 2.8)

add_library(Environment MODULE "Environment.cpp")
link_plugin(Environment)