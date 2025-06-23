#define DUCKDB_EXTENSION_MAIN

#include "quickjs_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/main/extension/extension_loader.hpp"
#include <duckdb/parser/parsed_data/create_scalar_function_info.hpp>

#include "quickjs.h"

namespace duckdb {

static void QuickJSExecute(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &script_vector = args.data[0];
	UnaryExecutor::Execute<string_t, string_t>(script_vector, result, args.size(), [&](string_t script) {
		JSRuntime *rt = JS_NewRuntime();
		JSContext *ctx = JS_NewContext(rt);
		JSValue val = JS_Eval(ctx, script.GetData(), script.GetSize(), "<eval>", JS_EVAL_TYPE_GLOBAL);

		string_t result_str;
		if (JS_IsException(val)) {
			JSValue exception = JS_GetException(ctx);
			const char *exception_str = JS_ToCString(ctx, exception);
			result_str = StringVector::AddString(result, exception_str);
			JS_FreeCString(ctx, exception_str);
			JS_FreeValue(ctx, exception);
		} else {
			const char *str = JS_ToCString(ctx, val);
			result_str = StringVector::AddString(result, str);
			JS_FreeCString(ctx, str);
		}

		JS_FreeValue(ctx, val);
		JS_FreeContext(ctx);
		JS_FreeRuntime(rt);
		return result_str;
	});
}

static void LoadInternal(ExtensionLoader &loader) {
	loader.SetDescription("QuickJS embedded scripting language");

	auto quickjs_scalar_function =
	    ScalarFunction("quickjs", {LogicalType::VARCHAR}, LogicalType::VARCHAR, QuickJSExecute);
	loader.RegisterFunction(quickjs_scalar_function);
}

void QuickjsExtension::Load(ExtensionLoader &loader) {
	LoadInternal(loader);
}
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

DUCKDB_CPP_EXTENSION_ENTRY(quickjs, loader) {
	duckdb::LoadInternal(loader);
}
}
