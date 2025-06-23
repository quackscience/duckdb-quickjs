
# DuckDB QuickJS Extension

This extension provides an embedded [QuickJS](https://github.com/quickjs-ng/quickjs) engine for [DuckDB](https://duckdb.org/). It allows you to execute JavaScript code directly within your SQL queries. QuickJS is a small, fast, and embeddable JavaScript engine that supports modern JavaScript features including ES2020.

## Functions

### Scalar `quickjs(code)`
Executes JavaScript code and returns the result as a string.

```sql
SELECT quickjs('2 + 2');
-- Returns: 4

SELECT quickjs('let msg = "Hello, World!"; msg');
-- Returns: Hello, World!
```

**Note:** For string literals with nested quotes, assign them to variables first to avoid parser confusion.

### Scalar `quickjs_eval(function, ...args)`
Executes a JavaScript function with the provided arguments and returns the result as JSON.

```sql
SELECT quickjs_eval('(a, b) => a + b', 5, 3);
-- Returns: 8
```

### Table `quickjs(code, ...args)`
Executes JavaScript code that returns an array and returns each array element as a separate row in a table. Accepts optional parameters that can be passed to the JavaScript function.

```sql
-- Simple array
SELECT * FROM quickjs('[1, 2, 3, 4, 5]');
-- Returns:
-- 1
-- 2
-- 3
-- 4
-- 5

-- With parameters
SELECT * FROM quickjs('parsed_arg0.map(x => x * arg1)', '[1, 2, 3, 4, 5]', 3);
-- Returns:
-- 3
-- 6
-- 9
-- 12
-- 15
```

**Parameter Naming Convention:**
- `arg0`, `arg1`, `arg2`, etc. - Access to the raw parameters passed to the function
- `parsed_arg0` - The first parameter parsed as JSON (if it's a string). This is useful when passing arrays or objects as the first parameter.

## Features

- **Full JavaScript ES2020 support**: Modern JavaScript features, arrow functions, template literals, etc.
- **Type conversion**: Automatic conversion between DuckDB and JavaScript data types
- **Error handling**: JavaScript errors are properly propagated as DuckDB exceptions
- **State isolation**: Each function call is completely isolated, preventing state leaks between calls

## Examples

For comprehensive examples of all function types and use cases, see the test file: [`test/sql/quickjs.test`](test/sql/quickjs.test)

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

Load the extension in DuckDB:

```sql
LOAD 'path/to/quickjs.duckdb_extension';
```

Then you can use any of the functions described above.

## Error Handling

The extension provides detailed error messages for JavaScript execution errors:

```sql
SELECT quickjs_eval('(a) => a.nonExistentMethod()', 1);
-- Throws: TypeError: a.nonExistentMethod is not a function
```

## Requirements

- DuckDB 1.3.1 or later
- C++11 compatible compiler
- CMake 3.16 or later

## Running Tests

To run the SQL tests for the extension, use the following command:

```sh
make test
```

## License

This project is licensed under the MIT License. See the [LICENSE](./LICENSE) file for details.

- [DuckDB](https://github.com/duckdb/duckdb) is available under the MIT License.
- [QuickJS-NG](https://github.com/quickjs-ng/quickjs) is available under the MIT License.
