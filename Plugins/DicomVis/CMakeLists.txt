cmake_minimum_required (VERSION 2.8)

add_library(DicomVis MODULE "DicomVis.cpp")
link_plugin(DicomVis)