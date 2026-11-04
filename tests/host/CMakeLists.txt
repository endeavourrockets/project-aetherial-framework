# Unity v2.6.1, copied from https://github.com/ThrowTheSwitch/Unity
# Revision cbcd08fa7de711053a3deec6339ee89cad5d2697. License is alongside src/.
add_library(unity STATIC ../../third_party/Unity/src/unity.c)
target_include_directories(unity PUBLIC ../../third_party/Unity/src)

add_executable(test_pressure
    test_pressure.c
    ../../firmware/obc/App/pressure.c)
target_include_directories(test_pressure PRIVATE ../../firmware/obc/App)
target_link_libraries(test_pressure PRIVATE unity)

add_test(NAME pressure COMMAND test_pressure)
set_tests_properties(pressure PROPERTIES TIMEOUT 5)
