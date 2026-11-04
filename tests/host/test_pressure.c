#include "unity.h"
#include "pressure.h"

void setUp(void) {}
void tearDown(void) {}

static void test_normal_pressure_is_valid(void)
{
    TEST_ASSERT_TRUE(pressure_is_valid(101325));
}

static void test_limits_are_excluded(void)
{
    TEST_ASSERT_FALSE(pressure_is_valid(1000));
    TEST_ASSERT_TRUE(pressure_is_valid(1001));
    TEST_ASSERT_TRUE(pressure_is_valid(119999));
    TEST_ASSERT_FALSE(pressure_is_valid(120000));
}

static void test_out_of_range_pressure_is_invalid(void)
{
    TEST_ASSERT_FALSE(pressure_is_valid(-1));
    TEST_ASSERT_FALSE(pressure_is_valid(0));
    TEST_ASSERT_FALSE(pressure_is_valid(INT32_MIN));
    TEST_ASSERT_FALSE(pressure_is_valid(INT32_MAX));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_normal_pressure_is_valid);
    RUN_TEST(test_limits_are_excluded);
    RUN_TEST(test_out_of_range_pressure_is_invalid);
    return UNITY_END();
}
