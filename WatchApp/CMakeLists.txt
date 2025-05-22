cmake_minimum_required(VERSION 3.13)
project(watch)
set(CMAKE_BUILD_TYPE Debug)

set(SOURCE_FILES    "entry.c"
                    "utils.c"
                    "framebuffer.c"
                    "config_handler.c"
                    "ntp_handler.c"
                    "input_handler.c"
                    "screens/charging_screen.c"
                    "screens/shutdown_screen.c"
                    "network_handler.c"
                    "duck_watchface.c" # MOVE AND COMPLETE
                    "watchfaces/uwoslab_serbio.c"
                    "watchfaces/basic_digital.c"
                    )

add_executable(watch ${SOURCE_FILES})

find_package(PkgConfig REQUIRED)
pkg_check_modules(CAIRO REQUIRED cairo)
pkg_check_modules(PANGO REQUIRED pango pangoft2 freetype2 pangocairo)
pkg_check_modules(CJSON REQUIRED libcjson)

include_directories(
                        ${CAIRO_INCLUDE_DIRS}
                        ${PANGO_INCLUDE_DIRS} ${CJSON_INCLUDE_DIRS}
                        )

target_link_libraries(watch
                        ${CAIRO_LIBRARIES}
                        ${PANGO_LIBRARIES}
                        ${CJSON_LIBRARIES}
                        )
