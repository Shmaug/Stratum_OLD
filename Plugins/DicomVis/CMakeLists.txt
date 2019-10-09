cmake_minimum_required (VERSION 3.8)

add_library(DicomVis SHARED "DicomVis.cpp")
link_plugin(DicomVis)