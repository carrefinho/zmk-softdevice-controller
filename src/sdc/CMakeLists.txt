if(CONFIG_ZMK_BT_LL_SOFTDEVICE)

zephyr_library()

get_filename_component(MODULE_ROOT ${CMAKE_CURRENT_LIST_DIR}/../.. ABSOLUTE)
set(NRFXLIB_DIR ${MODULE_ROOT}/nrfxlib)

if(NOT EXISTS ${NRFXLIB_DIR})
  message(FATAL_ERROR "nrfxlib not found at ${NRFXLIB_DIR}. Run 'west update'.")
endif()

if(CONFIG_SOC_NRF52840)
  set(SDC_SOC_FAMILY "nrf52")
else()
  message(FATAL_ERROR "Unsupported SoC. Only nRF52840 is currently supported.")
endif()

if(CONFIG_FPU)
  set(SDC_FLOAT_ABI "hard-float")
else()
  set(SDC_FLOAT_ABI "soft-float")
endif()

# Include paths
zephyr_include_directories(
  ${NRFXLIB_DIR}/softdevice_controller/include
  ${NRFXLIB_DIR}/mpsl/include
  ${NRFXLIB_DIR}/mpsl/include/protocol
  ${NRFXLIB_DIR}/mpsl/fem/include
  include
  controller
  mpsl
)

# MPSL clock control must come BEFORE hal_nordic to override nrfx_power_clock.h
target_include_directories(zephyr_interface BEFORE INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/clock_control
)

# Source files
zephyr_library_sources(
  controller/hci_driver.c
  controller/hci_internal.c
  controller/hci_internal_wrappers.c
  mpsl/mpsl_init.c
  mpsl/multithreading_lock.c
  clock_control/nrfx_clock_mpsl.c
)

zephyr_library_sources_ifdef(CONFIG_BT_CTLR_CRYPTO controller/crypto.c)

# Binary libraries
set(SDC_LIB_DIR ${NRFXLIB_DIR}/softdevice_controller/lib/${SDC_SOC_FAMILY}/${SDC_FLOAT_ABI})
set(MPSL_LIB_DIR ${NRFXLIB_DIR}/mpsl/lib/${SDC_SOC_FAMILY}/${SDC_FLOAT_ABI})
set(MPSL_FEM_DIR ${NRFXLIB_DIR}/mpsl/fem/common/lib/${SDC_SOC_FAMILY}/${SDC_FLOAT_ABI})

if(CONFIG_ZMK_SDC_MULTIROLE)
  set(SDC_VARIANT "multirole")
elseif(CONFIG_ZMK_SDC_PERIPHERAL)
  set(SDC_VARIANT "peripheral")
elseif(CONFIG_ZMK_SDC_CENTRAL)
  set(SDC_VARIANT "central")
else()
  set(SDC_VARIANT "multirole")
endif()

zephyr_link_libraries(${SDC_LIB_DIR}/libsoftdevice_controller_${SDC_VARIANT}.a)
zephyr_link_libraries(${MPSL_LIB_DIR}/libmpsl.a)
zephyr_link_libraries(${MPSL_FEM_DIR}/libmpsl_fem_common.a)

message(STATUS "ZMK SoftDevice Controller:")
message(STATUS "  SoC: ${SDC_SOC_FAMILY}, Float: ${SDC_FLOAT_ABI}, Variant: ${SDC_VARIANT}")
message(STATUS "  SDC: ${SDC_LIB_DIR}/libsoftdevice_controller_${SDC_VARIANT}.a")
message(STATUS "  MPSL: ${MPSL_LIB_DIR}/libmpsl.a")

endif()
