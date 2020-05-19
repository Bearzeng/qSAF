cmake_minimum_required(VERSION 3.0)

option( PLUGIN_STANDARD_QSAF "Check to install qSAF plugin" OFF )

if (PLUGIN_STANDARD_QSAF)

    #CloudCompare ‘SAF’ plugin

    project( QSAF_PLUGIN )
    AddPlugin( NAME ${PROJECT_NAME} )

    #include( CMakePluginTemplate )

    target_sources( ${PROJECT_NAME}
            PRIVATE
                    ${CMAKE_CURRENT_LIST_DIR}/qSAF.h
                    ${CMAKE_CURRENT_LIST_DIR}/qSAF.cpp
                    ${CMAKE_CURRENT_LIST_DIR}/SAFDlg.ui
                    ${CMAKE_CURRENT_LIST_DIR}/ccSAFDlg.h
                    ${CMAKE_CURRENT_LIST_DIR}/ccSAFDlg.cpp
    )
    
endif()
