#pragma once

#include <vector>
#include <string>
#include <algorithm>
#include <stdexcept>
#include <cctype>

namespace tests {
	int RunBigDecimalTests(bool verbose);
};

class BigDecimal {
public:
	BigDecimal() : negative(false), scale(0) {
		digits.push_back(0);
	}

	explicit BigDecimal(const std::string& s) {
		parseFromString(s);
	}

	BigDecimal(long long v) {
		if (v < 0) {
			negative = true;
			v = -v;
		}
		else {
			negative = false;
		}
		scale = 0;
		if (v == 0) {
			digits = { 0 };
		}
		else {
			std::string tmp = std::to_string(v);
			digits.clear();
			for (char c : tmp) digits.push_back(c - '0');
		}
	}

	friend BigDecimal operator+(const BigDecimal& a, const BigDecimal& b) {
		BigDecimal r = a;
		r += b;
		return r;
	}

	friend BigDecimal operator-(const BigDecimal& a, const BigDecimal& b) {
		BigDecimal r = a;
		r -= b;
		return r;
	}

	friend BigDecimal operator*(const BigDecimal& a, const BigDecimal& b) {
		return multiply(a, b);
	}

	friend BigDecimal operator/(const BigDecimal& a, const BigDecimal& b) {
		return divide(a, b, 20);
	}

	BigDecimal& operator+=(const BigDecimal& other) {
		addOrSubtract(other, true);
		return *this;
	}

	BigDecimal& operator-=(const BigDecimal& other) {
		addOrSubtract(other, false);
		return *this;
	}

	bool isZero() const {
		return digits.size() == 1 && digits[0] == 0;
	}

	std::string toString() const {
		if (isZero()) return "0";

		std::string s;

		if (negative && !isZero())
			s.push_back('-');

		int n = static_cast<int>(digits.size());
		int integerDigits = n - scale;

		if (integerDigits <= 0) {
			s += "0.";
			for (int i = 0; i < -integerDigits; ++i)
				s.push_back('0');
			for (int d : digits)
				s.push_back(static_cast<char>('0' + d));
		}
		else {
			int idx = 0;
			for (; idx < integerDigits; ++idx)
				s.push_back(static_cast<char>('0' + digits[idx]));

			if (scale > 0) {
				s.push_back('.');
				for (; idx < n; ++idx)
					s.push_back(static_cast<char>('0' + digits[idx]));
			}
		}

		if (scale > 0) {
			while (!s.empty() && s.back() == '0')
				s.pop_back();
			if (!s.empty() && s.back() == '.')
				s.pop_back();
		}

		if (s == "" || s == "-" || s == "-0")
			return "0";

		return s;
	}

private:
	std::vector<int> digits;
	bool negative;
	int scale;

	void parseFromString(const std::string& str) {
		digits.clear();
		negative = false;
		scale = 0;

		std::string s;
		size_t start = 0, end = str.size();
		while (start < end && std::isspace(static_cast<unsigned char>(str[start]))) ++start;
		while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1]))) --end;
		if (start >= end) {
			throw std::invalid_argument("Empty numeric string");
		}
		s = str.substr(start, end - start);

		if (s[0] == '+' || s[0] == '-') {
			negative = (s[0] == '-');
			s.erase(s.begin());
		}

		if (s.empty()) {
			throw std::invalid_argument("Empty numeric string after sign");
		}

		size_t dotPos = s.find('.');
		if (dotPos != std::string::npos) {
			if (s.find('.', dotPos + 1) != std::string::npos) {
				throw std::invalid_argument("Multiple decimal points");
			}
			scale = static_cast<int>(s.size() - dotPos - 1);
			s.erase(dotPos, 1);
		}
		else {
			scale = 0;
		}

		bool hasDigit = false;
		for (char c : s) {
			if (c < '0' || c > '9') {
				throw std::invalid_argument("Invalid character in numeric string");
			}
			digits.push_back(c - '0');
			hasDigit = true;
		}

		if (!hasDigit) {
			throw std::invalid_argument("No digits in numeric string");
		}

		size_t firstNonZero = 0;
		while (firstNonZero < digits.size() && digits[firstNonZero] == 0)
			++firstNonZero;

		if (firstNonZero == digits.size()) {
			digits.assign(1, 0);
			scale = 0;
			negative = false;
		}
		else if (firstNonZero > 0) {
			digits.erase(digits.begin(), digits.begin() + static_cast<long>(firstNonZero));
		}
	}


	void trimLeadingZeros() {
		size_t firstNonZero = 0;
		while (firstNonZero < digits.size() && digits[firstNonZero] == 0)
			++firstNonZero;
		if (firstNonZero == digits.size()) {
			digits.assign(1, 0);
			scale = 0;
			negative = false;
		}
		else if (firstNonZero > 0) {
			digits.erase(digits.begin(), digits.begin() + static_cast<long>(firstNonZero));
		}
	}

	static void alignScales(BigDecimal& a, BigDecimal& b) {
		if (a.scale == b.scale) return;
		if (a.scale < b.scale) {
			int diff = b.scale - a.scale;
			a.digits.insert(a.digits.end(), diff, 0);
			a.scale = b.scale;
		}
		else {
			int diff = a.scale - b.scale;
			b.digits.insert(b.digits.end(), diff, 0);
			b.scale = a.scale;
		}
	}

	static int compareAbs(const BigDecimal& a, const BigDecimal& b) {
		if (a.digits.size() != b.digits.size())
			return (a.digits.size() < b.digits.size()) ? -1 : 1;

		for (size_t i = 0; i < a.digits.size(); ++i) {
			if (a.digits[i] != b.digits[i])
				return (a.digits[i] < b.digits[i]) ? -1 : 1;
		}
		return 0;
	}

	static void padLeft(std::vector<int>& a, std::vector<int>& b) {
		if (a.size() < b.size()) {
			a.insert(a.begin(), b.size() - a.size(), 0);
		}
		else if (b.size() < a.size()) {
			b.insert(b.begin(), a.size() - b.size(), 0);
		}
	}

	static std::vector<int> addVectors(const std::vector<int>& a, const std::vector<int>& b) {
		std::vector<int> ra = a;
		std::vector<int> rb = b;
		padLeft(ra, rb);

		int n = static_cast<int>(ra.size());
		std::vector<int> res(n);
		int carry = 0;
		for (int i = n - 1; i >= 0; --i) {
			int sum = ra[i] + rb[i] + carry;
			res[i] = sum % 10;
			carry = sum / 10;
		}
		if (carry) res.insert(res.begin(), carry);
		return res;
	}

	static std::vector<int> subtractVectors(const std::vector<int>& a, const std::vector<int>& b) {
		std::vector<int> ra = a;
		std::vector<int> rb = b;
		padLeft(ra, rb);

		int n = static_cast<int>(ra.size());
		std::vector<int> res(n);
		int borrow = 0;
		for (int i = n - 1; i >= 0; --i) {
			int diff = ra[i] - rb[i] - borrow;
			if (diff < 0) {
				diff += 10;
				borrow = 1;
			}
			else {
				borrow = 0;
			}
			res[i] = diff;
		}

		size_t firstNonZero = 0;
		while (firstNonZero + 1 < res.size() && res[firstNonZero] == 0)
			++firstNonZero;
		if (firstNonZero > 0)
			res.erase(res.begin(), res.begin() + static_cast<long>(firstNonZero));

		return res;
	}


	void addOrSubtract(const BigDecimal& other, bool isAddition) {
		BigDecimal lhs = *this;
		BigDecimal rhs = other;
		alignScales(lhs, rhs);

		if (!isAddition) {
			rhs.negative = !rhs.negative;
		}

		if (lhs.negative == rhs.negative) {
			digits = addVectors(lhs.digits, rhs.digits);
			negative = lhs.negative;
			scale = lhs.scale;
		}
		else {
			int cmp = compareAbs(lhs, rhs);
			if (cmp == 0) {
				digits.assign(1, 0);
				scale = 0;
				negative = false;
			}
			else if (cmp > 0) {
				digits = subtractVectors(lhs.digits, rhs.digits);
				negative = lhs.negative;
				scale = lhs.scale;
			}
			else {
				digits = subtractVectors(rhs.digits, lhs.digits);
				negative = rhs.negative;
				scale = lhs.scale;
			}
		}

		trimLeadingZeros();
	}

	static BigDecimal multiply(const BigDecimal& a, const BigDecimal& b) {
		if (a.isZero() || b.isZero())
			return BigDecimal("0");

		BigDecimal res;
		res.negative = (a.negative != b.negative);
		res.scale = a.scale + b.scale;

		int n = static_cast<int>(a.digits.size());
		int m = static_cast<int>(b.digits.size());
		std::vector<int> tmp(n + m, 0);

		for (int i = n - 1; i >= 0; --i) {
			int carry = 0;
			for (int j = m - 1; j >= 0; --j) {
				int idx = i + j + 1;
				int prod = a.digits[i] * b.digits[j] + tmp[idx] + carry;
				tmp[idx] = prod % 10;
				carry = prod / 10;
			}
			tmp[i] += carry;
		}

		size_t firstNonZero = 0;
		while (firstNonZero + 1 < tmp.size() && tmp[firstNonZero] == 0)
			++firstNonZero;
		if (firstNonZero > 0)
			tmp.erase(tmp.begin(), tmp.begin() + static_cast<long>(firstNonZero));

		res.digits = std::move(tmp);
		if (res.isZero()) {
			res.negative = false;
			res.scale = 0;
		}
		return res;
	}

	static BigDecimal divide(const BigDecimal& numerator,
		const BigDecimal& denominator,
		int precision) {
		if (denominator.isZero())
			throw std::runtime_error("Division by zero");

		BigDecimal a = numerator;
		BigDecimal b = denominator;

		alignScales(a, b);

		int totalScale = a.scale - b.scale;
		a.scale = 0;
		b.scale = 0;

		if (precision > 0) {
			a.digits.insert(a.digits.end(), precision, 0);
			totalScale += precision;
		}

		a.negative = false;
		b.negative = false;

		BigDecimal current("0");
		BigDecimal res;
		res.digits.clear();
		res.negative = (numerator.negative != denominator.negative);
		res.scale = totalScale;

		for (size_t i = 0; i < a.digits.size(); ++i) {
			if (!current.isZero()) {
				current.digits.push_back(a.digits[i]);
			}
			else {
				current.digits[0] = a.digits[i];
			}
			current.trimLeadingZeros();
			current.scale = 0;

			int qDigit = 0;
			for (int d = 9; d >= 1; --d) {
				BigDecimal t = b * BigDecimal(d);
				if (compareAbs(t, current) <= 0) {
					qDigit = d;
					current = current - t;
					break;
				}
			}
			res.digits.push_back(qDigit);
		}

		res.trimLeadingZeros();
		if (res.isZero()) {
			res.scale = 0;
			res.negative = false;
		}
		return res;
	}
};
