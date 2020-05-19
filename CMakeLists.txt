cmake_minimum_required(VERSION 3.0)

option( PLUGIN_STANDARD_QSAF "Check to install qSAF plugin" OFF )

if (PLUGIN_STANDARD_QSAF)

    #CloudCompare ‘SAF’ plugin

    project( QSAF_PLUGIN )
    
    include( CMakePluginTemplate )
    
endif()
