# ---------------------------------------------------------------------------
# Split UDR library.
#
# The original code was created by Simonov Denis
# for UDR open source full-text search library for Firebird DBMS.
#
# Copyright (c) 2022 Simonov Denis <sim-mail@list.ru>
# and all contributors signed below.
#
# All Rights Reserved.
# Contributor(s): ______________________________________.
# ----------------------------------------------------------------------------

cmake_minimum_required (VERSION 3.13)

project (splitudr 
    VERSION 1.1.2
	LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

set(FIREBIRD_DIR /opt/firebird)
set(FIREBIRD_LIB_DIR ${FIREBIRD_DIR}/lib)
set(FIREBIRD_UDR_DIR ${FIREBIRD_DIR}/plugins/udr)

####################################
# library target
####################################
add_library(splitudr SHARED ${splitudr_sources})

####################################
# src
####################################
target_sources(splitudr PRIVATE "src/SplitProcedure.cpp")

####################################
# include directories
####################################
target_include_directories(splitudr PRIVATE
    ${FIREBIRD_INCLUDE_DIR}
)

# require C++17 standard
target_compile_features(splitudr PRIVATE cxx_std_17)
target_compile_definitions(splitudr PRIVATE HAVE_CONFIG_H)
target_compile_options(splitudr PRIVATE
#    -Werror
    -Wall
    -Wextra
    -Wpedantic
    -Wno-unused-parameter
)


install(TARGETS splitudr DESTINATION ${FIREBIRD_UDR_DIR})
install(FILES "sql/split-udr.sql"
              DESTINATION ${FIREBIRD_UDR_DIR})

