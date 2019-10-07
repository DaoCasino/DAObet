#include <gtest/gtest.h>

using namespace std;

constexpr uint32_t random_seed = 42;

int main(int argc, char **argv) {
    srand(random_seed);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
