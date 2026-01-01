if(CONFIG_ZMK_BT_LL_SOFTDEVICE)
  add_subdirectory(src/sdc)
endif()

if(CONFIG_ZMK_BT_LL_SOFTDEVICE AND CONFIG_BT_SUBRATING)
  zephyr_library_sources(src/subrating.c)
  zephyr_include_directories(${APPLICATION_SOURCE_DIR}/include)
endif()
