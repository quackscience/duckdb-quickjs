[![Extension Test](https://github.com/isaacbrodsky/duckdb-lua/actions/workflows/MainDistributionPipeline.yml/badge.svg)](https://github.com/isaacbrodsky/duckdb-lua/actions/workflows/MainDistributionPipeline.yml)
[![DuckDB Version](https://img.shields.io/static/v1?label=duckdb&message=v1.3.1&color=blue)](https://github.com/duckdb/duckdb/releases/tag/v1.3.1)
[![QuickJS Version](https://img.shields.io/static/v1?label=quickjs&message=v2024-01-13&color=blue)](https://github.com/quickjs-ng/quickjs)
[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE)

# DuckDB QuickJS Extension

This extension provides an embedded [QuickJS](https://github.com/quickjs-ng/quickjs) engine for [DuckDB](https://duckdb.org/). It allows you to execute JavaScript code directly within your SQL queries. QuickJS is a small, fast, and embeddable JavaScript engine that supports modern JavaScript features including ES2020.

## Features

- **`quickjs()` function**: Execute arbitrary JavaScript code and return the result as a string
- **`quickjs_eval()` function**: Execute JavaScript functions with DuckDB parameters and return JSON results
- **Full JavaScript ES2020 support**: Modern JavaScript features, arrow functions, template literals, etc.
- **Type conversion**: Automatic conversion between DuckDB and JavaScript data types
- **Error handling**: JavaScript errors are properly propagated as DuckDB exceptions

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

### Basic JavaScript Execution with `quickjs()`

The `quickjs()` function executes JavaScript code and returns the result as a string:

```sql
-- Simple arithmetic
SELECT quickjs('1 + 2');
-- Result: 3

-- String operations
SELECT quickjs('"hello" + " " + "world"');
-- Result: hello world

-- String methods
SELECT quickjs('"test".toUpperCase()');
-- Result: TEST

-- Template literals
SELECT quickjs('`The answer is ${42}`');
-- Result: The answer is 42

-- Array operations
SELECT quickjs('[1,2,3,4,5].reduce((a,b) => a + b, 0)');
-- Result: 15

-- Date formatting
SELECT quickjs('new Date().toISOString().split("T")[0]');
-- Result: 2024-01-15 (current date)
```

### Function Execution with `quickjs_eval()`

The `quickjs_eval()` function allows you to pass DuckDB values as parameters to JavaScript functions and returns JSON results:

```sql
-- Simple function with parameters
SELECT quickjs_eval('(a,b,c) => a + b + c', 1, 5, 4);
-- Result: 10

-- String concatenation with parameters
SELECT quickjs_eval('(a,b) => a + " " + b', 'hello', 'world');
-- Result: "hello world"

-- Mathematical operations
SELECT quickjs_eval('(a,b) => a * b', 3.5, 2);
-- Result: 7

-- Array operations with parameters
SELECT quickjs_eval('(arr) => arr.filter(x => x > 2).map(x => x * 2)', '[1,2,3,4,5]');
-- Result: [6,8,10]

-- Object creation
SELECT quickjs_eval('(name, age) => ({name, age, greeting: `Hello ${name}!`})', 'Alice', 30);
-- Result: {"name":"Alice","age":30,"greeting":"Hello Alice!"}

-- Complex calculations
SELECT quickjs_eval('(values) => {
  const nums = JSON.parse(values);
  return {
    sum: nums.reduce((a,b) => a + b, 0),
    avg: nums.reduce((a,b) => a + b, 0) / nums.length,
    max: Math.max(...nums),
    min: Math.min(...nums)
  }
}', '[10,20,30,40,50]');
-- Result: {"sum":150,"avg":30,"max":50,"min":10}
```

### Working with DuckDB Data

You can use QuickJS functions to process data from your DuckDB tables:

```sql
-- Create a sample table
CREATE TABLE users AS SELECT * FROM (VALUES 
  ('Alice', 25, 'Engineer'),
  ('Bob', 30, 'Designer'),
  ('Charlie', 35, 'Manager')
) t(name, age, role);

-- Process table data with JavaScript
SELECT 
  name,
  age,
  quickjs_eval('(age, role) => age > 30 ? "Senior " + role : role', age, role) as enhanced_role
FROM users;

-- Result:
-- name     | age | enhanced_role
-- Alice    | 25  | Engineer
-- Bob      | 30  | Designer  
-- Charlie  | 35  | Senior Manager

-- Complex data transformation
SELECT 
  name,
  quickjs_eval('(name, age, role) => ({
    id: name.toLowerCase().replace(" ", "_"),
    displayName: name.toUpperCase(),
    category: age > 30 ? "experienced" : "junior",
    role: role
  })', name, age, role) as user_info
FROM users;
```

### Error Handling

JavaScript errors are properly handled and converted to DuckDB exceptions:

```sql
-- This will throw a TypeError
SELECT quickjs_eval('(a,b) => a.nonExistentMethod()', 1, 2);
-- Error: TypeError: a.nonExistentMethod is not a function

-- This will throw a ReferenceError
SELECT quickjs('undefinedVariable');
-- Error: ReferenceError: undefinedVariable is not defined
```

### Advanced Examples

```sql
-- JSON processing
SELECT quickjs_eval('(jsonStr) => {
  const data = JSON.parse(jsonStr);
  return data.items
    .filter(item => item.price > 100)
    .map(item => ({...item, discounted: item.price * 0.9}))
    .sort((a,b) => b.price - a.price);
}', '{"items":[{"name":"Laptop","price":1200},{"name":"Mouse","price":50},{"name":"Monitor","price":300}]}');

-- Date manipulation
SELECT quickjs_eval('(dateStr) => {
  const date = new Date(dateStr);
  return {
    year: date.getFullYear(),
    month: date.getMonth() + 1,
    day: date.getDate(),
    dayOfWeek: ["Sun","Mon","Tue","Wed","Thu","Fri","Sat"][date.getDay()],
    isWeekend: [0,6].includes(date.getDay())
  }
}', '2024-01-15');

-- Text processing
SELECT quickjs_eval('(text) => {
  const words = text.toLowerCase().split(/\\W+/).filter(w => w.length > 0);
  const wordCount = {};
  words.forEach(word => wordCount[word] = (wordCount[word] || 0) + 1);
  return Object.entries(wordCount)
    .sort(([,a], [,b]) => b - a)
    .slice(0, 5)
    .map(([word, count]) => `${word}:${count}`);
}', 'The quick brown fox jumps over the lazy dog. The fox is quick and brown.');
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
