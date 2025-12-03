#include "json.hpp"

namespace tests
{
	using namespace jsonh;

	bool run_single_roundtrip_test(std::mt19937& rng) {
		JsonValue original = random_json(rng);
		std::string json = to_string(original);

		try {
			JsonParser parser(json);
			JsonValue parsed = parser.parse();
			if (!(original == parsed)) {
				std::cerr << "Roundtrip mismatch!\n";
				std::cerr << "Compact JSON: " << json << "\n";
				std::cerr << "Pretty JSON:\n" << to_string_pretty(original, 2) << "\n";
				return false;
			}
		}
		catch (const std::exception& e) {
			std::cerr << "Exception while parsing: " << e.what() << "\n";
			std::cerr << "JSON: " << json << "\n";
			return false;
		}
		return true;
	}

	bool run_invalid_json_tests() {
		std::vector<std::string> invalid_cases = {
			"",
			"nul",
			"tru",
			"fal",
			"{",
			"[",
			"\"abc",
			"{ \"a\" }",
			"{ \"a\": }",
			"{ \"a\": 1, }",
			"[1, 2, ]",
			"{ 123: \"x\" }",
			"[1 2]",
			"00",
			"01",
			"--1",
			"1e",
			"\"\\uZZZZ\"",
		};

		bool all_failed_as_expected = true;
		for (const auto& js : invalid_cases) {
			try {
				JsonParser parser(js);
				JsonValue parsed = parser.parse();
				std::cerr << "ERROR: Expected failure but parsed successfully: \"" << js << "\"\n";
				(void)parsed;
				all_failed_as_expected = false;
			}
			catch (const std::exception&) {
			}
		}

		return all_failed_as_expected;
	}

	bool run_schema_tests() {
		using Obj = JsonValue::object;

		Obj person;
		person["name"] = JsonValue("Alice");
		person["age"] = JsonValue(30.0);
		person["admin"] = JsonValue(true);
		JsonValue person_val(person);

		std::vector<FieldRequirement> schema = {
			{"name",  JsonType::String, false},
			{"age",   JsonType::Number, false},
			{"admin", JsonType::Bool,   true}
		};

		std::string error;
		if (!validate_object_schema(person_val, schema, error)) {
			std::cerr << "Schema test failed on valid object: " << error << "\n";
			return false;
		}

		if (!has_key(person_val, "name") || !has_key(person_val, "age")) {
			std::cerr << "has_key failed on existing keys\n";
			return false;
		}

		if (has_key(person_val, "nonexistent")) {
			std::cerr << "has_key returned true for missing key\n";
			return false;
		}

		Obj person_no_age = person;
		person_no_age.erase("age");
		JsonValue val_no_age(person_no_age);
		error.clear();
		if (validate_object_schema(val_no_age, schema, error)) {
			std::cerr << "Schema test: expected failure (missing 'age') but passed\n";
			return false;
		}

		Obj person_age_str = person;
		person_age_str["age"] = JsonValue("thirty");
		JsonValue val_age_str(person_age_str);
		error.clear();
		if (validate_object_schema(val_age_str, schema, error)) {
			std::cerr << "Schema test: expected failure (wrong type for 'age') but passed\n";
			return false;
		}

		return true;
	}

	bool run_view_tests() {
		using Obj = JsonValue::object;

		Obj o;
		o["name"] = JsonValue("Bob");
		o["age"] = JsonValue(40.0);
		o["tags"] = JsonValue(JsonValue::array{ JsonValue("dev"), JsonValue("c++") });
		JsonValue root(o);

		try {
			JsonObjectView view(root);

			if (!view.has("name") || !view.has("age") || view.has("missing")) {
				std::cerr << "JsonObjectView::has failed\n";
				return false;
			}

			std::string name = view.get<std::string>("name");
			double      age = view.get<double>("age");

			if (name != "Bob" || age != 40.0) {
				std::cerr << "JsonObjectView::get returned wrong values\n";
				return false;
			}

			auto tags_opt = view.get_optional<JsonValue::array>("tags");
			if (!tags_opt || tags_opt->size() != 2) {
				std::cerr << "JsonObjectView::get_optional failed for existing key\n";
				return false;
			}

			auto missing_opt = view.get_optional<double>("missing");
			if (missing_opt.has_value()) {
				std::cerr << "JsonObjectView::get_optional should be nullopt for missing key\n";
				return false;
			}

			bool wrong_type_threw = false;
			try {
				view.get<double>("name");
			}
			catch (...) {
				wrong_type_threw = true;
			}
			if (!wrong_type_threw) {
				std::cerr << "JsonObjectView::get did not throw on wrong type\n";
				return false;
			}

		}
		catch (const std::exception& e) {
			std::cerr << "JsonObjectView tests threw: " << e.what() << "\n";
			return false;
		}

		JsonValue not_obj(123.0);
		bool constructed = true;
		try {
			JsonObjectView bad_view(not_obj);
			(void)bad_view;
		}
		catch (...) {
			constructed = false;
		}
		if (constructed) {
			std::cerr << "JsonObjectView should throw if constructed from non-object\n";
			return false;
		}

		return true;
	}

	bool run_mut_view_tests() {
		using Obj = JsonValue::object;

		Obj o;
		o["name"] = JsonValue("Bob");
		JsonValue root(o);

		try {
			JsonObjectViewMut mut(root);

			if (!mut.has("name") || mut.has("age")) {
				std::cerr << "JsonObjectViewMut::has incorrect\n";
				return false;
			}

			mut.set("age", 40.0);
			mut.set("admin", true);
			mut.set("nickname", "Bobby");

			if (!mut.has("age") || !mut.has("admin") || !mut.has("nickname")) {
				std::cerr << "JsonObjectViewMut::set did not create keys\n";
				return false;
			}

			mut.erase("admin");
			if (mut.has("admin")) {
				std::cerr << "JsonObjectViewMut::erase failed\n";
				return false;
			}

			JsonObjectView view(root);
			double age = view.get<double>("age");
			std::string nick = view.get<std::string>("nickname");
			if (age != 40.0 || nick != "Bobby") {
				std::cerr << "JsonObjectViewMut changes not visible in view\n";
				return false;
			}

		}
		catch (const std::exception& e) {
			std::cerr << "JsonObjectViewMut tests threw: " << e.what() << "\n";
			return false;
		}

		JsonValue not_obj(123.0);
		bool constructed = true;
		try {
			JsonObjectViewMut bad(not_obj);
			(void)bad;
		}
		catch (...) {
			constructed = false;
		}
		if (constructed) {
			std::cerr << "JsonObjectViewMut should throw for non-object\n";
			return false;
		}

		return true;
	}


	int RunJsonTests(bool verbose) {
		std::random_device rd;
		std::mt19937 rng(rd());

		int failures = 0;

		if (!run_invalid_json_tests()) {
			std::cerr << "Invalid JSON tests failed.\n";
			++failures;
		}
		else {
			std::cout << "All invalid JSON tests passed.\n";
		}

		if (!run_schema_tests()) {
			std::cerr << "Schema/exists tests failed.\n";
			++failures;
		}
		else {
			std::cout << "All schema/exists tests passed.\n";
		}

		if (!run_view_tests()) {
			std::cerr << "JsonObjectView tests failed.\n";
			++failures;
		}
		else {
			std::cout << "All JsonObjectView tests passed.\n";
		}

		if (!run_mut_view_tests()) {
			std::cerr << "JsonObjectViewMut tests failed.\n";
			++failures;
		}
		else {
			std::cout << "All JsonObjectViewMut tests passed.\n";
		}

		const int NUM_TESTS = 1000;
		int random_failures = 0;
		for (int i = 0; i < NUM_TESTS; ++i) {
			if (!run_single_roundtrip_test(rng)) {
				++random_failures;
				break;
			}
		}

		if (random_failures == 0) {
			std::cout << "All " << NUM_TESTS << " random roundtrip tests passed.\n";
		}
		else {
			std::cout << random_failures << " random roundtrip tests failed.\n";
			++failures;
		}

		JsonValue demo = random_json(rng);
		std::cout << "\nDemo compact JSON:\n" << to_string(demo) << "\n";
		std::cout << "\nDemo pretty JSON:\n" << to_string_pretty(demo, 2) << "\n";

		if (failures == 0) {
			std::cout << "\nOverall: ALL TESTS PASSED.\n";
			return 0;
		}
		else {
			std::cout << "\nOverall: SOME TESTS FAILED.\n";
			return 1;
		}
	}
};