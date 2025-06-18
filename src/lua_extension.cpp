#define DUCKDB_EXTENSION_MAIN

#include "lua_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/main/extension/extension_loader.hpp"
#include <duckdb/parser/parsed_data/create_scalar_function_info.hpp>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

namespace duckdb {

inline void LuaScalarFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &script_vector = args.data[0];
	UnaryExecutor::Execute<string_t, string_t>(script_vector, result, args.size(), [&](string_t script) {
		lua_State *L = luaL_newstate();
		luaL_openlibs(L);

		auto error = luaL_loadbuffer(L, script.GetData(), script.GetSize(), "line") || lua_pcall(L, 0, 0, 0);
		std::string resultStr;
		if (error) {
			// TODO: error
			fprintf(stderr, "%s\n", lua_tostring(L, -1));
			lua_pop(L, 1);
		} else {
			resultStr = "ok";
		}

		lua_close(L);

		return StringVector::AddString(result, "Lua " + resultStr + " 🐥");
	});
}

static void LoadInternal(ExtensionLoader &loader) {
	loader.SetDescription(StringUtil::Format("Lua embedded scripting language, %s", LUA_RELEASE));

	auto lua_scalar_function = ScalarFunction("lua", {LogicalType::VARCHAR}, LogicalType::VARCHAR, LuaScalarFun);
	loader.RegisterFunction(lua_scalar_function);
}

void LuaExtension::Load(ExtensionLoader &loader) {
	LoadInternal(loader);
}
std::string LuaExtension::Name() {
	return "lua";
}

std::string LuaExtension::Version() const {
#ifdef EXT_VERSION_LUA
	return EXT_VERSION_LUA;
#else
	return "";
#endif
}

} // namespace duckdb

extern "C" {

DUCKDB_CPP_EXTENSION_ENTRY(lua, loader) {
	duckdb::LoadInternal(loader);
}
}
