#pragma once

#include "duckdb.hpp"

#ifdef DUCKDB_CPP_EXTENSION_ENTRY
#include "duckdb/main/extension/extension_loader.hpp"
#endif

namespace duckdb {

class QuickjsExtension : public Extension {
public:
#ifdef DUCKDB_CPP_EXTENSION_ENTRY
	void Load(ExtensionLoader &loader) override;
#else
	void Load(DuckDB &db) override;
#endif
	std::string Name() override;
	std::string Version() const override;
};

} // namespace duckdb
