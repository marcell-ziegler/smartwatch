#include "framework.h"

// Each test unit exposes one entry function.
void test_clocktime();
void test_csv_utils();
void test_time_parsing();
void test_converters();
void test_record_parsers();
void test_validation();
void test_findtrain();

int main()
{
    test_clocktime();
    test_csv_utils();
    test_time_parsing();
    test_converters();
    test_record_parsers();
    test_validation();
    test_findtrain();

    std::printf("\n%d checks, %d failure(s)\n", tf::checks, tf::failures);
    return tf::failures == 0 ? 0 : 1;
}
