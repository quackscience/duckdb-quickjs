#define DUCKDB_EXTENSION_MAIN

#include "quickjs_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/scalar_function.hpp"
#include <duckdb/parser/parsed_data/create_scalar_function_info.hpp>
#include "duckdb/common/types/value.hpp"

#ifndef DUCKDB_CPP_EXTENSION_ENTRY
#include "duckdb/main/extension_util.hpp"
#endif

#include "quickjs.h"

namespace duckdb {

static JSValue DuckDBValueToJSValue(JSContext *ctx, const Value &val) {
	if (val.IsNull()) {
		return JS_NULL;
	}
	switch (val.type().id()) {
	case LogicalTypeId::BOOLEAN:
		return JS_NewBool(ctx, val.GetValue<bool>());
	case LogicalTypeId::INTEGER:
		return JS_NewInt32(ctx, val.GetValue<int32_t>());
	case LogicalTypeId::BIGINT:
		return JS_NewInt64(ctx, val.GetValue<int64_t>());
	case LogicalTypeId::FLOAT:
		return JS_NewFloat64(ctx, val.GetValue<float>());
	case LogicalTypeId::DOUBLE:
		return JS_NewFloat64(ctx, val.GetValue<double>());
	case LogicalTypeId::VARCHAR:
		return JS_NewString(ctx, val.GetValue<string>().c_str());
	default:
		return JS_NewString(ctx, val.ToString().c_str());
	}
}

static void QuickJSEval(DataChunk &args, ExpressionState &state, Vector &result) {
	auto count = args.size();

	Vector &script_vector = args.data[0];
	script_vector.Flatten(count);
	auto script_data = FlatVector::GetData<string_t>(script_vector);
	auto &script_validity = FlatVector::Validity(script_vector);

	result.SetVectorType(VectorType::FLAT_VECTOR);
	auto result_data = FlatVector::GetData<string_t>(result);
	auto &result_validity = FlatVector::Validity(result);

	for (idx_t i = 0; i < count; i++) {
		if (!script_validity.RowIsValid(i)) {
			result_validity.SetInvalid(i);
			continue;
		}

		JSRuntime *rt = JS_NewRuntime();
		if (!rt) {
			throw IOException("Failed to create QuickJS runtime.");
		}
		JSContext *ctx = JS_NewContext(rt);
		if (!ctx) {
			JS_FreeRuntime(rt);
			throw IOException("Failed to create QuickJS context.");
		}

		auto script = script_data[i];
		JSValue func = JS_Eval(ctx, script.GetData(), script.GetSize(), "<eval>", JS_EVAL_TYPE_GLOBAL);

		if (JS_IsException(func)) {
			JSValue exception = JS_GetException(ctx);
			const char *exception_c_str = JS_ToCString(ctx, exception);
			std::string exception_str(exception_c_str);
			JS_FreeCString(ctx, exception_c_str);
			JS_FreeValue(ctx, exception);
			JS_FreeValue(ctx, func);
			JS_FreeContext(ctx);
			JS_FreeRuntime(rt);
			throw InvalidInputException(exception_str);
		}

		if (!JS_IsFunction(ctx, func)) {
			JS_FreeValue(ctx, func);
			JS_FreeContext(ctx);
			JS_FreeRuntime(rt);
			throw InvalidInputException("First argument to js_eval must be a function");
		}

		idx_t n_js_args = args.ColumnCount() - 1;
		std::vector<JSValue> js_args;
		js_args.reserve(n_js_args);
		for (idx_t j = 0; j < n_js_args; j++) {
			auto val = args.data[j + 1].GetValue(i);
			js_args.push_back(DuckDBValueToJSValue(ctx, val));
		}

		JSValue global_obj = JS_GetGlobalObject(ctx);
		JSValue js_result = JS_Call(ctx, func, global_obj, n_js_args, js_args.data());

		for (auto &arg : js_args) {
			JS_FreeValue(ctx, arg);
		}
		JS_FreeValue(ctx, func);
		JS_FreeValue(ctx, global_obj);

		if (JS_IsException(js_result)) {
			JSValue exception = JS_GetException(ctx);
			const char *exception_c_str = JS_ToCString(ctx, exception);
			std::string exception_str(exception_c_str);
			JS_FreeCString(ctx, exception_c_str);
			JS_FreeValue(ctx, exception);
			JS_FreeValue(ctx, js_result);
			JS_FreeContext(ctx);
			JS_FreeRuntime(rt);
			throw InvalidInputException(exception_str);
		}

		JSValue json_string_val = JS_JSONStringify(ctx, js_result, JS_UNDEFINED, JS_UNDEFINED);
		JS_FreeValue(ctx, js_result);

		if (JS_IsException(json_string_val)) {
			JSValue exception = JS_GetException(ctx);
			const char *exception_c_str = JS_ToCString(ctx, exception);
			std::string exception_str(exception_c_str);
			JS_FreeCString(ctx, exception_c_str);
			JS_FreeValue(ctx, exception);
			JS_FreeValue(ctx, json_string_val);
			JS_FreeContext(ctx);
			JS_FreeRuntime(rt);
			throw InvalidInputException("Failed to stringify result to JSON: %s", exception_str);
		}

		size_t len;
		const char *json_c_str = JS_ToCStringLen(ctx, &len, json_string_val);
		result_data[i] = StringVector::AddString(result, json_c_str, len);
		JS_FreeCString(ctx, json_c_str);
		JS_FreeValue(ctx, json_string_val);

		JS_FreeContext(ctx);
		JS_FreeRuntime(rt);
	}
}

static void QuickJSExecute(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &script_vector = args.data[0];
	UnaryExecutor::Execute<string_t, string_t>(script_vector, result, args.size(), [&](string_t script) {
		JSRuntime *rt = JS_NewRuntime();
		JSContext *ctx = JS_NewContext(rt);
		JSValue val = JS_Eval(ctx, script.GetData(), script.GetSize(), "<eval>", JS_EVAL_TYPE_GLOBAL);

		if (JS_IsException(val)) {
			JSValue exception = JS_GetException(ctx);
			const char *exception_c_str = JS_ToCString(ctx, exception);
			std::string exception_str(exception_c_str);
			JS_FreeCString(ctx, exception_c_str);

			JS_FreeValue(ctx, exception);
			JS_FreeValue(ctx, val);
			JS_FreeContext(ctx);
			JS_FreeRuntime(rt);

			throw InvalidInputException(exception_str);
		}

		const char *c_str = JS_ToCString(ctx, val);
		string_t result_str = StringVector::AddString(result, c_str);
		JS_FreeCString(ctx, c_str);

		JS_FreeValue(ctx, val);
		JS_FreeContext(ctx);
		JS_FreeRuntime(rt);
		return result_str;
	});
}

#ifdef DUCKDB_CPP_EXTENSION_ENTRY
static void LoadInternal(ExtensionLoader &loader) {
	loader.SetDescription("QuickJS embedded scripting language");

	auto quickjs_scalar_function =
	    ScalarFunction("quickjs", {LogicalType::VARCHAR}, LogicalType::VARCHAR, QuickJSExecute);
	loader.RegisterFunction(quickjs_scalar_function);

	auto quickjs_eval_function =
	    ScalarFunction("quickjs_eval", {LogicalType::VARCHAR}, LogicalType::JSON(), QuickJSEval);
	quickjs_eval_function.varargs = LogicalType::ANY;
	loader.RegisterFunction(quickjs_eval_function);
}
#else
static void LoadInternal(DatabaseInstance &instance) {
	auto quickjs_scalar_function =
	    ScalarFunction("quickjs", {LogicalType::VARCHAR}, LogicalType::VARCHAR, QuickJSExecute);
	ExtensionUtil::RegisterFunction(instance, quickjs_scalar_function);

	auto quickjs_eval_function =
	    ScalarFunction("quickjs_eval", {LogicalType::VARCHAR}, LogicalType::JSON(), QuickJSEval);
	quickjs_eval_function.varargs = LogicalType::ANY;
	ExtensionUtil::RegisterFunction(instance, quickjs_eval_function);
}
#endif

#ifdef DUCKDB_CPP_EXTENSION_ENTRY
void QuickjsExtension::Load(ExtensionLoader &loader) {
	LoadInternal(loader);
}
#else
void QuickjsExtension::Load(DuckDB &db) {
	LoadInternal(*db.instance);
}
#endif

std::string QuickjsExtension::Name() {
	return "quickjs";
}

std::string QuickjsExtension::Version() const {
#ifdef EXT_VERSION_QUICKJS
	return EXT_VERSION_QUICKJS;
#else
	return "";
#endif
}

} // namespace duckdb

extern "C" {

#ifdef DUCKDB_CPP_EXTENSION_ENTRY
DUCKDB_CPP_EXTENSION_ENTRY(quickjs, loader) {
	duckdb::LoadInternal(loader);
}
#else
DUCKDB_EXTENSION_API void quickjs_init(duckdb::DatabaseInstance &db) {
	duckdb::DuckDB db_wrapper(db);
	db_wrapper.LoadExtension<duckdb::QuickjsExtension>();
}
#endif

DUCKDB_EXTENSION_API const char *quickjs_version() {
	return duckdb::DuckDB::LibraryVersion();
}
}
