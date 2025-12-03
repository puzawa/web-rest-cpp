#include "web/http_server/http_server.hpp"
#include "bigdec/bigdec.hpp"
#include "json/json.hpp"

#include <pqxx/pqxx>


int main() {
	tests::RunBigDecimalTests(false);
	tests::RunJsonTests(true);
	tests::RunHttpServerTests(true);
}