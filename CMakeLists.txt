cmake_minimum_required(VERSION 3.10)
project(ISP_AMS_Canny)

set(CMAKE_CXX_STANDARD 17)
find_package(Threads REQUIRED)

# Installation paths
set(SYSTEMC_INC       /home/tjsw/systemc-2.3.3-install/include)
set(SYSTEMC_LIBDIR    /home/tjsw/systemc-2.3.3-install/lib-linux64)
set(SYSTEMC_AMS_INC   /home/tjsw/systemc-ams-2.3/include)
set(SYSTEMC_AMS_LIBDIR /home/tjsw/systemc-ams-2.3/lib-linux64)
set(VERILATOR_INCLUDE /usr/share/verilator/include)
set(VERILATOR_OBJ_DIR /home/tjsw/git1/obj_dir)

include_directories(
  ${SYSTEMC_INC}
  ${SYSTEMC_AMS_INC}
  ${VERILATOR_INCLUDE}   # for verilated.h
  ${VERILATOR_OBJ_DIR}   # for VCannyEdge.h
)

link_directories(
  ${SYSTEMC_LIBDIR}
  ${SYSTEMC_AMS_LIBDIR}
)

add_executable(isp_pipeline_ams
  main.cpp
  BMPUtils.cpp
  CannyEdgeWrapper.cpp
  IdentityLUT.cpp
  Sensor.cpp
  BurstPacker.cpp
  LPDDR.cpp
  ISP_Canny.cpp
  Lut1D_DE.cpp
  PcieDMA_Tap.cpp
  ReadSink256.cpp

  # Verilator minimal runtime (vendored, not the whole install)
  third_party/verilator_runtime/verilated.cpp
  third_party/verilator_runtime/verilated_vcd_c.cpp
)

# Includes
target_include_directories(isp_pipeline_ams PRIVATE
  ${SYSTEMC_INC}
  ${SYSTEMC_AMS_INC}
  ${VERILATOR_INCLUDE}                              # /usr/share/verilator/include
  ${CMAKE_SOURCE_DIR}/third_party/verilator_runtime # headers for the minimal runtime
  ${CMAKE_SOURCE_DIR}/third_party/canny/obj_dir     # <- symlink to lab10/src/obj_dir
)

# Link SystemC/AMS
target_link_libraries(isp_pipeline_ams
  ${SYSTEMC_LIBDIR}/libsystemc.a
  ${SYSTEMC_AMS_LIBDIR}/libsystemc-ams.a
  Threads::Threads
)

# Use ALL generated Verilated sources (newer Verilator splits files)
file(GLOB VERILATED_MODEL_SRCS
  ${CMAKE_SOURCE_DIR}/third_party/canny/obj_dir/VCannyEdge*.cpp
  ${CMAKE_SOURCE_DIR}/third_party/canny/obj_dir/VCannyEdge__*.cpp
  ${CMAKE_SOURCE_DIR}/third_party/canny/obj_dir/VCannyEdge___*.cpp
)
target_sources(isp_pipeline_ams PRIVATE ${VERILATED_MODEL_SRCS})

set_target_properties(isp_pipeline_ams PROPERTIES
  BUILD_WITH_INSTALL_RPATH TRUE
  INSTALL_RPATH "${SYSTEMC_LIBDIR}:${SYSTEMC_AMS_LIBDIR}"
)

