#include "bigdec/bigdec.hpp"
#include "json/json.hpp"

int main() {
	tests::RunBigDecimalTests(false);
	tests::RunJsonTests(false);
}