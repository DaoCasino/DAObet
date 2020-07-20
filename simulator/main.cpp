#include "log.hpp"

#include "eosio/randpa_plugin/randpa_logger.hpp"

#include <fc/log/logger.hpp>
#include <fc/log/logger_config.hpp>

#include <gtest/gtest.h>

using namespace std;

void init_randpa_logger() {
    auto cfg = fc::logging_config::default_config();
    cfg.loggers.push_back(cfg.loggers[0]);
    cfg.loggers.back().name = randpa_finality::randpa_logger_name;
    fc::log_config::configure_logging(cfg);
}

int main(int argc, char **argv) {
    auto seed = time(NULL);
    std::srand(seed);
    std::cout << "Random number generator seeded to " << seed << std::endl;
    init_randpa_logger();

    bool is_verbose = false;
    std::string verbose_arg = "--verbose";
    for (int i = 0; i < argc; i++) {
       if (verbose_arg == argv[i]) {
          is_verbose = true;
          break;
       }
    }

    if (is_verbose) {
        fc::logger::get(randpa_finality::randpa_logger_name).set_log_level(fc::log_level::debug);
    } else {
        std::cout << "Logging disabled, use --verbose to enable" << std::endl;
        logger.enabled(false);
        fc::logger::get(randpa_finality::randpa_logger_name).set_log_level(fc::log_level::off);
    }

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
