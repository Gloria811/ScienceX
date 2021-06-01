cmake_minimum_required(VERSION 3.8.2)
set(WORKSPACE_DIR ${CMAKE_SOURCE_DIR}/../.. CACHE PATH "root of the PID workspace directory")
list(APPEND CMAKE_MODULE_PATH ${WORKSPACE_DIR}/cmake) # using generic scripts/modules of the workspace
include(Package_Definition NO_POLICY_SCOPE)

project(xcontrol)

PID_Package(
    AUTHOR             Xplore
    ADDRESS https://github.com/Xavier0301/xcontrol.git

    YEAR               2021
    LICENSE            CeCILL
    DESCRIPTION        xcontrol enabled motor control at Xplore
    CONTRIBUTION_SPACE xcontrol_contrib
    VERSION            0.1.0
)

PID_Dependency(ethercatcpp-core VERSION 1.1.1)
PID_Dependency(ethercatcpp-epos VERSION 1.0.3)

if(BUILD_EXAMPLES)
  PID_Dependency(pid-os-utilities VERSION 2.0)
endif()

build_PID_Package()
