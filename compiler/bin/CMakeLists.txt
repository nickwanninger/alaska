install(PROGRAMS alaska DESTINATION bin)
install(PROGRAMS alaska++ DESTINATION bin)
install(PROGRAMS alaska-transform DESTINATION bin)
install(PROGRAMS alaska-transform-baseline DESTINATION bin)

install(PROGRAMS alaska-clang DESTINATION bin)
install(PROGRAMS ld.alaska-clang DESTINATION bin)
# install(PROGRAMS alaska-config DESTINATION bin)

add_executable(alaska-config alaska-config.cpp)
install(TARGETS alaska-config)
