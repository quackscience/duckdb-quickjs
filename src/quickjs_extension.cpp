#define DUCKDB_EXTENSION_MAIN

#include "quickjs_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/scalar_function.hpp"
#include <duckdb/parser/parsed_data/create_scalar_function_info.hpp>
#include "duckdb/common/types/value.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/parser/parsed_data/create_table_function_info.hpp"

#ifndef DUCKDB_CPP_EXTENSION_ENTRY
#include "duckdb/main/extension_util.hpp"
#endif

#include "quickjs.h"

namespace duckdb {

// Add the bind data and global state classes
class QuickJSTableBindData : public TableFunctionData {
public:
	QuickJSTableBindData() {
	}
	
	std::string js_code;
	vector<Value> parameters;
};

class QuickJSTableGlobalState : public GlobalTableFunctionState {
public:
	QuickJSTableGlobalState() : current_index(0) {
	}

	vector<string> results;
	idx_t current_index;
};

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

static void QuickJSTableFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &bind_data = data_p.bind_data->Cast<QuickJSTableBindData>();
	auto &state = data_p.global_state->Cast<QuickJSTableGlobalState>();

	if (state.current_index >= state.results.size()) {
		output.SetCardinality(0);
		return;
	}

	idx_t chunk_size = MinValue<idx_t>(STANDARD_VECTOR_SIZE, state.results.size() - state.current_index);
	output.SetCardinality(chunk_size);

	// For now, we'll return a single column with the JSON string representation
	// In the future, we could parse the JSON and create multiple columns
	auto &result_vector = output.data[0];
	auto result_data = FlatVector::GetData<string_t>(result_vector);

	for (idx_t i = 0; i < chunk_size; i++) {
		result_data[i] = StringVector::AddString(result_vector, state.results[state.current_index + i]);
	}

	state.current_index += chunk_size;
}

static unique_ptr<FunctionData> QuickJSTableBind(ClientContext &context, TableFunctionBindInput &input,
                                                 vector<LogicalType> &return_types, vector<string> &names) {
	// Check that we have at least one argument (the JavaScript code)
	if (input.inputs.size() < 1) {
		throw BinderException("quickjs() expects at least one argument (JavaScript code)");
	}

	// For now, return a single JSON column
	return_types.push_back(LogicalType::JSON());
	names.push_back("result");

	auto bind_data = make_uniq<QuickJSTableBindData>();
	
	// Extract the JavaScript code from the first argument
	if (!input.inputs[0].IsNull()) {
		bind_data->js_code = input.inputs[0].GetValue<string>();
	} else {
		throw BinderException("quickjs() requires a non-null JavaScript code string");
	}

	// Store any additional parameters
	for (idx_t i = 1; i < input.inputs.size(); i++) {
		bind_data->parameters.push_back(input.inputs[i]);
	}

	return std::move(bind_data);
}

static unique_ptr<GlobalTableFunctionState> QuickJSTableInit(ClientContext &context, TableFunctionInitInput &input) {
	auto &bind_data = input.bind_data->Cast<QuickJSTableBindData>();
	auto result = make_uniq<QuickJSTableGlobalState>();

	// Execute the JavaScript code and collect results
	JSRuntime *rt = JS_NewRuntime();
	if (!rt) {
		throw IOException("Failed to create QuickJS runtime.");
	}
	JSContext *ctx = JS_NewContext(rt);
	if (!ctx) {
		JS_FreeRuntime(rt);
		throw IOException("Failed to create QuickJS context.");
	}

	// Create a function that takes the parameters and returns the result
	std::string js_function_code = "(function(";
	for (idx_t i = 0; i < bind_data.parameters.size(); i++) {
		if (i > 0) js_function_code += ", ";
		js_function_code += "arg" + std::to_string(i);
	}
	js_function_code += ") { ";
	
	// Parse JSON strings for the first parameter if it's a string (likely an array/object)
	if (!bind_data.parameters.empty() && bind_data.parameters[0].type() == LogicalType::VARCHAR) {
		js_function_code += "let parsed_arg0 = JSON.parse(arg0); ";
	}
	
	js_function_code += "return " + bind_data.js_code + "; })";

	// Compile the function
	JSValue func_val = JS_Eval(ctx, js_function_code.c_str(), js_function_code.length(), "<eval>", JS_EVAL_TYPE_GLOBAL);
	if (JS_IsException(func_val)) {
		JSValue exception = JS_GetException(ctx);
		const char *exception_c_str = JS_ToCString(ctx, exception);
		std::string exception_str(exception_c_str);
		JS_FreeCString(ctx, exception_c_str);
		JS_FreeValue(ctx, exception);
		JS_FreeValue(ctx, func_val);
		JS_FreeContext(ctx);
		JS_FreeRuntime(rt);
		throw InvalidInputException("Failed to compile JavaScript function: %s", exception_str);
	}

	// Convert parameters to JavaScript values
	vector<JSValue> js_args;
	for (const auto &param : bind_data.parameters) {
		js_args.push_back(DuckDBValueToJSValue(ctx, param));
	}

	// Call the function
	JSValue val = JS_Call(ctx, func_val, JS_UNDEFINED, js_args.size(), js_args.data());

	// Clean up function and arguments
	JS_FreeValue(ctx, func_val);
	for (auto &arg : js_args) {
		JS_FreeValue(ctx, arg);
	}

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

	// Check if the result is an array
	if (!JS_IsArray(val)) {
		JS_FreeValue(ctx, val);
		JS_FreeContext(ctx);
		JS_FreeRuntime(rt);
		throw InvalidInputException("JavaScript code must return an array");
	}

	// Get array length
	JSAtom length_atom = JS_NewAtom(ctx, "length");
	JSValue length_val = JS_GetProperty(ctx, val, length_atom);
	int32_t length = JS_VALUE_GET_INT(length_val);
	JS_FreeValue(ctx, length_val);
	JS_FreeAtom(ctx, length_atom);

	// Extract each array element as a separate row
	for (int32_t i = 0; i < length; i++) {
		JSValue element = JS_GetPropertyUint32(ctx, val, i);
		
		JSValue json_string_val = JS_JSONStringify(ctx, element, JS_UNDEFINED, JS_UNDEFINED);
		JS_FreeValue(ctx, element);

		if (JS_IsException(json_string_val)) {
			JSValue exception = JS_GetException(ctx);
			const char *exception_c_str = JS_ToCString(ctx, exception);
			std::string exception_str(exception_c_str);
			JS_FreeCString(ctx, exception_c_str);
			JS_FreeValue(ctx, exception);
			JS_FreeValue(ctx, json_string_val);
			JS_FreeValue(ctx, val);
			JS_FreeContext(ctx);
			JS_FreeRuntime(rt);
			throw InvalidInputException("Failed to stringify result to JSON: %s", exception_str);
		}

		size_t len;
		const char *json_c_str = JS_ToCStringLen(ctx, &len, json_string_val);
		std::string json_str(json_c_str, len);
		JS_FreeCString(ctx, json_c_str);
		JS_FreeValue(ctx, json_string_val);

		result->results.push_back(json_str);
	}

	JS_FreeValue(ctx, val);
	JS_FreeContext(ctx);
	JS_FreeRuntime(rt);

	return std::move(result);
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

	// Register the table function
	auto quickjs_table_function = TableFunction("quickjs", {LogicalType::VARCHAR}, QuickJSTableFunction, QuickJSTableBind, QuickJSTableInit);
	quickjs_table_function.varargs = LogicalType::ANY;
	loader.RegisterFunction(quickjs_table_function);
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

	// Register the table function
	auto quickjs_table_function = TableFunction("quickjs", {LogicalType::VARCHAR}, QuickJSTableFunction, QuickJSTableBind, QuickJSTableInit);
	quickjs_table_function.varargs = LogicalType::ANY;
	ExtensionUtil::RegisterFunction(instance, quickjs_table_function);
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
