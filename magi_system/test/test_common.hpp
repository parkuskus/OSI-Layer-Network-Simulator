#ifndef MAGI_SYSTEM_TEST_COMMON_HPP
#define MAGI_SYSTEM_TEST_COMMON_HPP

#include <functional>
#include <iostream>
#include <sstream>
#include <string>

namespace magi_test
{

    struct TestStats
    {
        int checks = 0;
        int failed = 0;
    };

    inline void expect(TestStats &stats, bool condition, const std::string &label)
    {
        ++stats.checks;
        if (condition)
        {
            std::cout << "    [OK] " << label << std::endl;
        }
        else
        {
            ++stats.failed;
            std::cout << "    [FAIL] " << label << std::endl;
        }
    }

    template <typename Fn>
    inline std::string captureStdout(Fn &&fn)
    {
        std::ostringstream buffer;
        std::streambuf *original = std::cout.rdbuf(buffer.rdbuf());
        try
        {
            fn();
        }
        catch (...)
        {
            std::cout.rdbuf(original);
            throw;
        }
        std::cout.rdbuf(original);
        return buffer.str();
    }

    inline bool contains(const std::string &text, const std::string &needle)
    {
        return text.find(needle) != std::string::npos;
    }

    inline void printSection(const std::string &title)
    {
        std::cout << std::endl;
        std::cout << "== " << title << " ==" << std::endl;
    }

} // namespace magi_test

#endif // MAGI_SYSTEM_TEST_COMMON_HPP
