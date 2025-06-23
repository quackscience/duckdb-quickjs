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

const auto CONTEXT_OPTION_NAME = "lua_context_name";

namespace duckdb {

inline void LuaScalarFun(DataChunk &args, ExpressionState &state, Vector &result) {
	Value contextVarNameValue = Value(LogicalType::VARCHAR);
	state.GetContext().TryGetCurrentSetting(CONTEXT_OPTION_NAME, contextVarNameValue);
	auto contextVarName = contextVarNameValue.GetValue<string>();

	auto &script_vector = args.data[0];
	UnaryExecutor::Execute<string_t, string_t>(script_vector, result, args.size(), [&](string_t script) {
		lua_State *L = luaL_newstate();
		luaL_openlibs(L);

		lua_pushnil(L);
		lua_setglobal(L, contextVarName.c_str());

		auto error = luaL_loadbuffer(L, script.GetData(), script.GetSize(), "line") || lua_pcall(L, 0, 1, 0);
		std::string resultStr;
		if (error) {
			resultStr = lua_tostring(L, -1);
			lua_pop(L, 1);
		} else {
			if (lua_isnoneornil(L, -1)) {
				resultStr = "nil";
				lua_pop(L, 1);
			} else if (lua_isstring(L, -1)) {
				resultStr = lua_tostring(L, -1);
				lua_pop(L, 1);
			} else if (lua_isinteger(L, -1)) {
				resultStr = StringUtil::Format("%d", lua_tointeger(L, -1));
				lua_pop(L, 1);
			} else if (lua_isnumber(L, -1)) {
				resultStr = StringUtil::Format("%f", lua_tonumber(L, -1));
				lua_pop(L, 1);
			} else if (lua_isboolean(L, -1)) {
				resultStr = lua_toboolean(L, -1) ? "true" : "false";
				lua_pop(L, 1);
			} else {
				resultStr = StringUtil::Format("Unknown type: %s", lua_typename(L, -1));
			}
		}

		lua_close(L);

		return StringVector::AddString(result, resultStr);
	});
}

inline void LuaScalar2Fun(DataChunk &args, ExpressionState &state, Vector &result) {
	Value contextVarNameValue = Value(LogicalType::VARCHAR);
	state.GetContext().TryGetCurrentSetting(CONTEXT_OPTION_NAME, contextVarNameValue);
	auto contextVarName = contextVarNameValue.GetValue<string>();

	auto &script_vector = args.data[0];
	auto &data_vector = args.data[1];
	BinaryExecutor::Execute<string_t, string_t, string_t>(
	    script_vector, data_vector, result, args.size(), [&](string_t script, string_t data) {
		    lua_State *L = luaL_newstate();
		    luaL_openlibs(L);

		    lua_pushlstring(L, data.GetData(), data.GetSize());
		    lua_setglobal(L, contextVarName.c_str());

		    auto error = luaL_loadbuffer(L, script.GetData(), script.GetSize(), "line") || lua_pcall(L, 0, 1, 0);
		    std::string resultStr;
		    if (error) {
			    resultStr = lua_tostring(L, -1);
			    lua_pop(L, 1);
		    } else {
			    if (lua_isnoneornil(L, -1)) {
				    resultStr = "nil";
				    lua_pop(L, 1);
			    } else if (lua_isstring(L, -1)) {
				    resultStr = lua_tostring(L, -1);
				    lua_pop(L, 1);
			    } else if (lua_isinteger(L, -1)) {
				    resultStr = StringUtil::Format("%d", lua_tointeger(L, -1));
				    lua_pop(L, 1);
			    } else if (lua_isnumber(L, -1)) {
				    resultStr = StringUtil::Format("%f", lua_tonumber(L, -1));
				    lua_pop(L, 1);
			    } else if (lua_isboolean(L, -1)) {
				    resultStr = lua_toboolean(L, -1) ? "true" : "false";
				    lua_pop(L, 1);
			    } else {
				    resultStr = StringUtil::Format("Unknown type: %s", lua_typename(L, -1));
			    }
		    }

		    lua_close(L);

		    return StringVector::AddString(result, resultStr);
	    });
}

static void LoadInternal(ExtensionLoader &loader) {
	loader.SetDescription(StringUtil::Format("Lua embedded scripting language, %s", LUA_RELEASE));

	auto lua_scalar_function = ScalarFunction("lua", {LogicalType::VARCHAR}, LogicalType::VARCHAR, LuaScalarFun);
	loader.RegisterFunction(lua_scalar_function);

	auto lua_scalar_function2 =
	    ScalarFunction("lua", {LogicalType::VARCHAR, LogicalType::VARCHAR}, LogicalType::VARCHAR, LuaScalar2Fun);
	loader.RegisterFunction(lua_scalar_function2);

	auto &config = DBConfig::GetConfig(loader.GetDatabaseInstance());
	config.AddExtensionOption("lua_context_name", "Global context variable name. Default: 'context'",
	                          LogicalType::VARCHAR, Value("context"));
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
