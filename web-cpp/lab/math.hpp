#pragma once
#include "bigdec/bigdec.hpp"

class HitChecker
{
	bool checkCircle(const BigDecimal& x, const BigDecimal& y, const BigDecimal& r) {
		BigDecimal zero(0ll);
		BigDecimal halfR = r / 2;

		bool inBounds =
			(x >= zero) &&
			(x <= halfR) &&
			(y >= zero) &&
			(y <= halfR);

		bool inCircle = (x * x + y * y) <= (halfR * halfR);

		return inBounds && inCircle;
	}

	bool checkRectangle(const BigDecimal& x, const BigDecimal& y, const BigDecimal& r) {
		BigDecimal zero(0ll);
		BigDecimal halfR = r / 2;
		BigDecimal minusR = -r;

		return (x <= zero) &&
			(x >= minusR) &&
			(y >= zero) &&
			(y <= halfR);
	}

	bool checkTriangle(const BigDecimal& x, const BigDecimal& y, const BigDecimal& r) {
		BigDecimal zero(0ll);
		BigDecimal halfR = r / 2;
		BigDecimal yMax = -(x * 2 + r);

		return 
			(x >= -halfR) &&
			(x <= zero) &&
			(y <= zero) &&
			(y >= yMax);
	}

public:

	bool hit_check(const std::string& x_str, const std::string& y_str, const std::string& r_str) {
		BigDecimal x(x_str), y(y_str), r(r_str);

		BigDecimal zero(0ll);
		if (r == zero)
			return false;

		if (r < zero)
			r = -r;

		return checkCircle(x, y, r) || checkRectangle(x, y, r) || checkTriangle(x, y, r);
	}
};