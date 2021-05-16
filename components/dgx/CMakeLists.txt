execute_process(COMMAND ${python} "../components/dgx/src/fontscan.py")

set(srcs "src/screen.c" "src/draw.c" "src/bw_screen.c" "src/v_screen.c" "src/bitmap.c" "src/font.c" "src/interscreen.c")

if(CONFIG_DGX_ENABLE_SPI)
	list(APPEND srcs "src/spi_screen.c")
endif()

if(CONFIG_DGX_ENABLE_I2C)
	list(APPEND srcs "src/i2c_screen.c")
endif()

if(CONFIG_DGX_ENABLE_SPI_ST7735)
	list(APPEND srcs "src/drivers/spi_st7735.c")
endif()

file(STRINGS "${BUILD_DIR}/../components/dgx/src/fonts/fonts.txt" fonts)

if(fonts)
	list(APPEND srcs ${fonts})
endif()

idf_component_register(SRCS "${srcs}" INCLUDE_DIRS "include")
