## sqlch
Code generator to read an SQL file and generate a C++ wrapper around the tables and statements.

# Quick Start
1. Download all files in this project
2. Compile sqlch.cpp on the command line as follows:
```
clang++ -std=c++17 sqlch.cpp -lsqlite3 -o sqlch
```
3. Put the generated binary "sqlch" anywhere in your PATH
4. Run it against the test.sql file
```
sqlch test.sql
```
5. Use the generated test.cpp and test.hpp in your code. For example, use it with testmain.cpp as below:
```
clang++ -std=c++17 testmain.cpp test.cpp -lsqlite3 -o test
```

NOTE: The sqlch executable is self-sufficient. In most cases, you won't need to "install" it on your system, or add it to your project. You will only need to copy the executable somewher on your PATH (say, /usr/local/bin) and invoke it from your project.

# Invoking in CMAKE
To generate the cpp/hpp files within a CMake project:
1. Add a custom command to your CMakeLists.txt file, as follows:
```
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/test.cpp" "${CMAKE_CURRENT_BINARY_DIR}/test.hpp"
    DEPENDS "test.sql"
    COMMENT "generating database access files"
    COMMAND sqlch -d ${CMAKE_CURRENT_BINARY_DIR}/ ${CMAKE_CURRENT_SOURCE_DIR}/test.sql
)
```
2. Add the generated cpp file to your source list
```
set(TEST_SQLCH_SOURCE
    testmain.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/test.cpp
)

add_executable(${PROJECT_NAME} ${TEST_SQLCH_SOURCE})
```

# Design
- **Database**: This class holds all code to create, open and close the connection to the underlying database
- **Interface**: This class holds a set of statements to access the database.  
You can define one or more interfaces to the database. This is useful in situations where you want some components of your app to update the database, and others to only read from it.  
Using interfaces, you can define separate read-write and read-only interfaces to the database and pass the appropriate references/pointers to the corresponding components in your app.

# Metacommands
Metacommands can be specified within single or multiline comments.
A multiline comment holding metacommands starts with a /** and ends with a **/
A single line comment is preceeded by 3 hyphens.

- **NAMESPACE**: This metacommand defines the namespace in which all generated classes should exist
- **HCODE**: Defines any additional code (declarations, functions, etc) that should be inserted at the beginning of the header file
- **SCODE**: Defines any additional code (declarations, functions, etc) that should be inserted at the beginning of the source file
- **INCLUDE**: Defines any header files that should be inserted at the beginning of the header file
- **IMPORT**: Defines any header files that should be inserted at the beginning of the source file
- **SQLCH**: Your app can have more than one .sql/.cpp/.hpp files. But the generated code contains a set of classes that are common across all the generated pairs of files, which will cause linker errors.  
Use `SQLCH OFF` to turn off the generation of these common classes in all files except one.
- **ON ERROR**: Define the function to be called on any error
- **ON TRACE**: Define the function to be called to trace database access
- **ON OPEN**: Define the function to be called before opening a database
- **ON OPENED**: Define the function to be called after opening a database
- **ENUM**: Use this to define mapping between enums and their string names. This will enable to store enumerations as strings in the database
- **VTYPE**: Use this to specify the native type of any field in a table. For example, should an INTEGER field be an `int` or a `uint64_t`, etc.
- **QNAME**: By default, the class for a select query is named as <tablename_selectfield1_selectfield2>. Use this to change the name of the class.
- **DEFINE DATABASE**: Use this to start defining a database. Typically this section will hold a set of CREATE TABLE commands.
- **END DATABASE**: Use this to end defining a database.
- **DEFINE INTERFACE**: Use this to start defining an interface.
- **END INTERFACE**: Use this to end defining an interface.


# TODO
- Need to implement cursors. At present, a SELECT query returns a vector of rows.
