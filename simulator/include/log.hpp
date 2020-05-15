#pragma once

#include <iostream>

class logger_type:
    public std::streambuf,
    public std::ostream
{
public:
    static logger_type& instance() {
        static logger_type inst;
        return inst;
    }

    void enabled(bool set) {
        _enabled = set;
    }

    bool enabled() const {
        return _enabled;
    }

    std::streambuf::int_type overflow(std::streambuf::int_type c) override {
        if (_enabled) {
            std::cout << static_cast<std::streambuf::char_type>(c);
        }
        return 0;
    }

private:
    logger_type():
        std::ostream(this)
    { }

private:
    bool _enabled { true };
};

static auto& logger = logger_type::instance();
