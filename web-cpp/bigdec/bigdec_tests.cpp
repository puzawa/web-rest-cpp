#include "bigdec.hpp"

#include <iostream>
#include <string>
#include <random>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <stdexcept>

namespace tests
{
	static bool g_verbose = false;


	void log_ok(const std::string& msg) {
		if (g_verbose) std::cout << "[OK] " << msg << "\n";
	}

	[[noreturn]] void fail(const std::string& msg) {
		std::cerr << "[FAIL] " << msg << "\n";
		std::abort();
	}

#define CHECK_STR(actual, expected)                                           \
    do {                                                                      \
        std::string _a = (actual);                                            \
        std::string _e = (expected);                                          \
        if (_a != _e) {                                                       \
            fail(std::string(#actual " == \"") + _e + "\", but was \"" + _a + "\""); \
        } else {                                                              \
            log_ok(std::string(#actual " == \"") + _e + "\"");                \
        }                                                                     \
    } while (0)

#define EXPECT_THROW_INVALID(expr)                                            \
    do {                                                                      \
        bool _thrown = false;                                                 \
        try {                                                                 \
            (void)(expr);                                                     \
        } catch (const std::invalid_argument&) {                              \
            _thrown = true;                                                   \
        } catch (...) {                                                       \
            fail(std::string("Wrong exception type for ") + #expr);           \
        }                                                                     \
        if (!_thrown) {                                                       \
            fail(std::string("Expected std::invalid_argument for ") + #expr); \
        } else {                                                              \
            log_ok(std::string("std::invalid_argument thrown as expected: ") + #expr); \
        }                                                                     \
    } while (0)


	long double toLongDouble(const BigDecimal& x) {
		return std::stold(x.toString());
	}

	bool almostEqual(long double a, long double b, long double relEps = 1e-12L) {
		long double diff = std::fabsl(a - b);
		long double maxab = std::max(std::fabsl(a), std::fabsl(b));
		if (maxab < 1.0L) maxab = 1.0L;
		return diff <= relEps * maxab;
	}

#define CHECK_ALMOST(actualBD, expectedLD, eps)                               \
    do {                                                                      \
        long double _val = toLongDouble(actualBD);                            \
        long double _ref = (expectedLD);                                      \
        if (!almostEqual(_val, _ref, (eps))) {                                \
            fail(std::string("almostEqual(" #actualBD ", " #expectedLD        \
                             ") failed: got ") + std::to_string((double)_val) \
                 + " expected " + std::to_string((double)_ref));              \
        } else {                                                              \
            log_ok(std::string("almostEqual(" #actualBD ", " #expectedLD ")")); \
        }                                                                     \
    } while (0)


	std::string randomDecimalString(std::mt19937& rng) {
		std::uniform_int_distribution<int> signDist(0, 1);
		std::uniform_int_distribution<int> digitsCountDist(1, 8);
		std::uniform_int_distribution<int> dotPosDist(0, 5);
		std::uniform_int_distribution<int> digitDist(0, 9);

		bool negative = (signDist(rng) == 1);
		int totalDigits = digitsCountDist(rng);
		int dotPos = dotPosDist(rng);

		std::string s;
		if (negative) s.push_back('-');

		for (int i = 0; i < totalDigits; ++i) {
			int d = digitDist(rng);
			if (i == 0 && totalDigits > 1 && d == 0) {
				d = 1;
			}
			s.push_back(char('0' + d));
		}

		if (dotPos < totalDigits) {
			size_t pos = 1 + (dotPos % totalDigits);
			if (pos >= s.size()) pos = s.size() - 1;
			if (pos > 0 && s[pos - 1] != '-' && s.find('.') == std::string::npos) {
				s.insert(pos, ".");
			}
		}

		return s;
	}


	void test_parsing_and_toString() {
		std::cout << "test_parsing_and_toString...\n";

		CHECK_STR(BigDecimal("0").toString(), "0");
		CHECK_STR(BigDecimal("000123").toString(), "123");
		CHECK_STR(BigDecimal("000123.4500").toString(), "123.45");
		CHECK_STR(BigDecimal("-0").toString(), "0");
		CHECK_STR(BigDecimal("-001.2300").toString(), "-1.23");
		CHECK_STR(BigDecimal("12345.67").toString(), "12345.67");
		CHECK_STR(BigDecimal("-0.0012300").toString(), "-0.00123");
		CHECK_STR(BigDecimal("0000.0000").toString(), "0");
		CHECK_STR(BigDecimal("0000.00100").toString(), "0.001");
		CHECK_STR(BigDecimal("-0000.00100").toString(), "-0.001");

		std::cout << "  OK\n";
	}

	void test_addition_subtraction() {
		std::cout << "test_addition_subtraction...\n";

		CHECK_STR((BigDecimal("1.5") + BigDecimal("2.25")).toString(), "3.75");
		CHECK_STR((BigDecimal("100.01") + BigDecimal("99.99")).toString(), "200");
		CHECK_STR((BigDecimal("-5.5") + BigDecimal("2.5")).toString(), "-3");
		CHECK_STR((BigDecimal("10") - BigDecimal("3")).toString(), "7");
		CHECK_STR((BigDecimal("3") - BigDecimal("10")).toString(), "-7");
		CHECK_STR((BigDecimal("-2.5") - BigDecimal("-2.5")).toString(), "0");

		CHECK_STR((BigDecimal("0.999") + BigDecimal("0.001")).toString(), "1");
		CHECK_STR((BigDecimal("1.000") - BigDecimal("0.001")).toString(), "0.999");

		CHECK_STR(
			(BigDecimal("123456789.123") + BigDecimal("876543210.877")).toString(),
			"1000000000"
		);
		CHECK_STR(
			(BigDecimal("1000000000") - BigDecimal("0.000000001")).toString(),
			"999999999.999999999"
		);

		std::cout << "  OK\n";
	}

	void test_multiplication() {
		std::cout << "test_multiplication...\n";

		CHECK_STR((BigDecimal("3") * BigDecimal("4")).toString(), "12");
		CHECK_STR((BigDecimal("1.5") * BigDecimal("2")).toString(), "3");
		CHECK_STR((BigDecimal("1.25") * BigDecimal("0.2")).toString(), "0.25");
		CHECK_STR((BigDecimal("-3.5") * BigDecimal("2")).toString(), "-7");
		CHECK_STR((BigDecimal("-3.5") * BigDecimal("-2")).toString(), "7");

		CHECK_STR(
			(BigDecimal("0.001") * BigDecimal("1000")).toString(),
			"1"
		);
		CHECK_STR(
			(BigDecimal("12345.678") * BigDecimal("0")).toString(),
			"0"
		);

		std::cout << "  OK\n";
	}

	void test_division_basic() {
		std::cout << "test_division_basic...\n";

		{
			BigDecimal a("10");
			BigDecimal b("2");
			BigDecimal c = a / b;
			CHECK_STR(c.toString(), "5");
		}
		{
			BigDecimal a("1");
			BigDecimal b("2");
			BigDecimal c = a / b;
			CHECK_ALMOST(c, 0.5L, 1e-15L);
		}
		{
			BigDecimal a("22");
			BigDecimal b("7");
			BigDecimal c = a / b;
			long double ref = 22.0L / 7.0L;
			CHECK_ALMOST(c, ref, 1e-10L);
		}
		{
			BigDecimal a("-5");
			BigDecimal b("2");
			BigDecimal c = a / b;
			long double ref = -5.0L / 2.0L;
			CHECK_ALMOST(c, ref, 1e-10L);
		}

		std::cout << "  OK\n";
	}

	void test_chained_ops() {
		std::cout << "test_chained_ops...\n";

		BigDecimal x("1.5");
		BigDecimal y("2.25");
		BigDecimal z("10");

		BigDecimal r = (x + y) * z - BigDecimal("5") / BigDecimal("2");
		CHECK_STR(r.toString(), "35");

		BigDecimal a("100.1");
		BigDecimal b("0.1");
		BigDecimal c("50");
		BigDecimal r2 = (a - b) / c;
		CHECK_STR(r2.toString(), "2");

		std::cout << "  OK\n";
	}


	void random_add_sub_mul_div_tests(unsigned int seed, int numTests) {
		std::cout << "random_add_sub_mul_div_tests... (seed=" << seed
			<< ", N=" << numTests << ")\n";

		std::mt19937 rng(seed);

		for (int i = 0; i < numTests; ++i) {
			std::string sa = randomDecimalString(rng);
			std::string sb = randomDecimalString(rng);

			BigDecimal A(sa);
			BigDecimal B(sb);

			long double a = 0, b = 0;
			try {
				a = std::stold(sa);
				b = std::stold(sb);
			}
			catch (...) {
				continue;
			}

			{
				BigDecimal C = A + B;
				long double ref = a + b;
				long double val = toLongDouble(C);
				if (!almostEqual(ref, val, 1e-10L)) {
					fail("Random add mismatch: " + sa + " + " + sb +
						" expected " + std::to_string((double)ref) +
						" got " + std::to_string((double)val));
				}
				else
					log_ok("Random div: " + sa + " / " + sb + " got " + std::to_string((double)val));
			}

			{
				BigDecimal C = A - B;
				long double ref = a - b;
				long double val = toLongDouble(C);
				if (!almostEqual(ref, val, 1e-10L)) {
					fail("Random sub mismatch: " + sa + " - " + sb +
						" expected " + std::to_string((double)ref) +
						" got " + std::to_string((double)val));
				}
				else
					log_ok("Random div: " + sa + " / " + sb + " got " + std::to_string((double)val));
			}

			{
				BigDecimal C = A * B;
				long double ref = a * b;
				long double val = toLongDouble(C);
				if (!almostEqual(ref, val, 1e-10L)) {
					fail("Random mul mismatch: " + sa + " * " + sb +
						" expected " + std::to_string((double)ref) +
						" got " + std::to_string((double)val));
				}
				else
					log_ok("Random div: " + sa + " / " + sb + " got " + std::to_string((double)val));
			}

			if (std::fabsl(b) > 1e-18L) {
				BigDecimal C = A / B;
				long double ref = a / b;
				long double val = toLongDouble(C);
				if (!almostEqual(ref, val, 1e-9L)) {
					fail("Random div mismatch: " + sa + " / " + sb +
						" expected " + std::to_string((double)ref) +
						" got " + std::to_string((double)val));
				}
				else
					log_ok("Random div: " + sa + " / " + sb + " got " + std::to_string((double)val));

			}

			if (g_verbose && (i % 100 == 0)) {
				std::cout << "  random test " << i << "/" << numTests << "...\n";
			}
		}

		std::cout << "  OK\n";
	}

	void test_invalid_input() {
		std::cout << "test_invalid_input...\n";

		EXPECT_THROW_INVALID(BigDecimal(""));
		EXPECT_THROW_INVALID(BigDecimal("   "));

		EXPECT_THROW_INVALID(BigDecimal("+"));
		EXPECT_THROW_INVALID(BigDecimal("-"));
		EXPECT_THROW_INVALID(BigDecimal("   +  "));
		EXPECT_THROW_INVALID(BigDecimal("   -   "));

		EXPECT_THROW_INVALID(BigDecimal("."));
		EXPECT_THROW_INVALID(BigDecimal(" . "));
		EXPECT_THROW_INVALID(BigDecimal("+."));
		EXPECT_THROW_INVALID(BigDecimal("-."));

		EXPECT_THROW_INVALID(BigDecimal("1.2.3"));
		EXPECT_THROW_INVALID(BigDecimal("..1"));
		EXPECT_THROW_INVALID(BigDecimal("1..0"));

		EXPECT_THROW_INVALID(BigDecimal("1a2"));
		EXPECT_THROW_INVALID(BigDecimal("abc"));
		EXPECT_THROW_INVALID(BigDecimal("--10"));
		EXPECT_THROW_INVALID(BigDecimal("++10"));
		EXPECT_THROW_INVALID(BigDecimal("1,23"));

		EXPECT_THROW_INVALID(BigDecimal("1 2 3"));
		EXPECT_THROW_INVALID(BigDecimal("1. 2"));
		EXPECT_THROW_INVALID(BigDecimal(" 1 . 2 "));

		CHECK_STR(BigDecimal("000").toString(), "0");
		CHECK_STR(BigDecimal("000.000").toString(), "0");
		CHECK_STR(BigDecimal(" +001.2300 ").toString(), "1.23");
		CHECK_STR(BigDecimal("  -000.00100 ").toString(), "-0.001");

		std::cout << "  OK\n";
	}



	int RunBigDecimalTests(bool verbose) {
		g_verbose = verbose;
		unsigned int seed = 123456u;
		int numRandomTests = 2000;

		try {
			test_parsing_and_toString();
			test_invalid_input();
			test_addition_subtraction();
			test_multiplication();
			test_division_basic();
			test_chained_ops();
			random_add_sub_mul_div_tests(seed, numRandomTests);
		}
		catch (const std::exception& ex) {
			std::cerr << "Exception in tests: " << ex.what() << "\n";
			return 1;
		}

		std::cout << "All BigDecimal tests passed!\n";
		return 0;
	}
};
