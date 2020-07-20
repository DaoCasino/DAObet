#pragma once

#include <fc/log/logger.hpp>
#include <fc/string.hpp>

namespace randpa_finality {

extern const fc::string randpa_logger_name;
extern fc::logger randpa_logger;

#define randpa_dlog(FORMAT, ...) \
    FC_MULTILINE_MACRO_BEGIN \
        if (randpa_logger.is_enabled(fc::log_level::debug)) \
            randpa_logger.log(FC_LOG_MESSAGE(debug, FORMAT, __VA_ARGS__)); \
    FC_MULTILINE_MACRO_END

#define randpa_ilog(FORMAT, ...) \
    FC_MULTILINE_MACRO_BEGIN \
        if (randpa_logger.is_enabled(fc::log_level::info)) \
            randpa_logger.log(FC_LOG_MESSAGE(info, FORMAT, __VA_ARGS__)); \
    FC_MULTILINE_MACRO_END

#define randpa_wlog(FORMAT, ...) \
    FC_MULTILINE_MACRO_BEGIN \
        if (randpa_logger.is_enabled(fc::log_level::warn)) \
            randpa_logger.log(FC_LOG_MESSAGE(warn, FORMAT, __VA_ARGS__)); \
    FC_MULTILINE_MACRO_END

#define randpa_elog(FORMAT, ...) \
    FC_MULTILINE_MACRO_BEGIN \
        if (randpa_logger.is_enabled(fc::log_level::error)) \
            randpa_logger.log( FC_LOG_MESSAGE(error, FORMAT, __VA_ARGS__)); \
    FC_MULTILINE_MACRO_END

} // randpa_finality
