[![Extension Test](https://github.com/isaacbrodsky/duckdb-lua/actions/workflows/MainDistributionPipeline.yml/badge.svg)](https://github.com/isaacbrodsky/duckdb-lua/actions/workflows/MainDistributionPipeline.yml)
[![DuckDB Version](https://img.shields.io/static/v1?label=duckdb&message=v1.3.1&color=blue)](https://github.com/duckdb/duckdb/releases/tag/v1.3.1)
[![Lua Version](https://img.shields.io/static/v1?label=lua&message=v5.4.8&color=blue)](https://lua.org/home.html)
[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE)

# DuckDB QuickJS Extension

This extension provides an embedded [QuickJS](https://github.com/quickjs-ng/quickjs) engine for [DuckDB](https://duckdb.org/). It allows you to execute JavaScript code directly within your SQL queries. QuickJS is a small, fast, and embeddable JavaScript engine.

## Building

To build the extension, you first need to clone this repository and initialize the necessary git submodules:

```sh
git clone --recursive https://github.com/your-repo/duckdb-quickjs.git
cd duckdb-quickjs
```

If you have already cloned the repository without the `--recursive` flag, you can initialize the submodules with:
```sh
git submodule update --init --recursive
```

Once the submodules are ready, you can build the extension using `make`:

```sh
make
```

The main binaries that will be built are:
```sh
./build/release/duckdb
./build/release/test/unittest
./build/release/extension/quickjs/quickjs.duckdb_extension
```
- `duckdb` is the binary for the DuckDB shell with the extension code automatically loaded.
- `unittest` is the test runner for DuckDB.
- `quickjs.duckdb_extension` is the loadable binary for the extension.

## Usage

You can use the `quickjs()` scalar function to execute JavaScript code. The function takes a string containing the script as an argument and returns the result as a string.

To run the extension, start the shell with `./build/release/duckdb`.

Here is an example of how to use the function:
```sql
D SELECT quickjs('1 + 2');
┌─────────┐
│ result  │
│ varchar │
├─────────┤
│ 3       │
└─────────┘

D SELECT quickjs('"hello" + " " + "world"');
┌───────────────┐
│    result     │
│    varchar    │
├───────────────┤
│ hello world   │
└───────────────┘
```

## Running Tests

To run the SQL tests for the extension, use the following command:

```sh
make test
```

## License

This project is licensed under the MIT License. See the [LICENSE](./LICENSE) file for details.

- [DuckDB](https://github.com/duckdb/duckdb) is available under the MIT License.
- [QuickJS-NG](https://github.com/quickjs-ng/quickjs) is available under the MIT License.
