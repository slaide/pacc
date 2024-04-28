#!/usr/bin/env python3

from dataclasses import dataclass
import subprocess as sp
import typing as tp
from pathlib import Path
import os
# ensure project root directory
print(Path(__file__).parent.parent)

@dataclass
class Test:
    file: str

    goal: tp.Optional[str] = None
    should_fail: bool = False

    def run(self):
        print(f"\033[1mRunning test: '{self.file}'\033[0m")
        command=f"bin/main {self.file}"

        # exec command
        result = sp.run(command, shell=True)
        if result.returncode == 0:
            print(f"\033[1;32mSuccess: '{command}'\033[0m")
        else:
            print(f"\033[1;31mError: '{command}' failed with exit code {result.returncode}\033[0m")
            
            exit(result.returncode)

tests=[
    Test(file="test/test001.c", goal="basic function definition"),
    Test(file="test/test002.c", goal="function definition with arguments"),
    Test(file="test/test003.c", goal="function definition with statements in body, single line comment before any code"),
    Test(file="test/test004.c", goal="multi line comment"),
    Test(file="test/test005.c", goal="string literal, variable declaration and assignment to string literal"),
    Test(file="test/test006.c", goal="variable declaration and assignment to integer literal"),
    Test(file="test/test007.c", goal="for loop and function call"),
    Test(file="test/test008.c", goal="preprocessor define directive, return non-literal value"),
    Test(file="test/test009.c", goal="preprocessor function-like define directive"),

    Test(file="test/test010.c", goal="include directive with quotation marks and angle brackets, function definition, for loop, arrow operator, dot operator"),
    Test(file="test/test011.c", goal="float literals and casting operation"),
    Test(file="test/test012.c", goal="type casting, array initializer, struct initializer, addrof operator, array subscript operator, arrow operator"),
    Test(file="test/test013.c", goal="explicit struct typename"),
    Test(file="test/test014.c", goal="const type"),
    Test(file="test/test015.c", goal="casting expression on struct initializer, typedef"),
    Test(file="test/test016.c", goal="if statement"),
    Test(file="test/test017.c", goal="array initializer"),
    Test(file="test/test018.c", goal="parens wrapped value operations"),
    Test(file="test/test019.c", goal="if statement with single statement in body, nested logical operators"),

    Test(file="test/test020.c", goal="if statement, array initializer"),
    Test(file="test/test021.c", goal="unary logical negation, unary binary negation"),
    Test(file="test/test022.c", goal="while loop, for loop"),
    Test(file="test/test023.c", goal="do while loop, multiple versions of a for loop"),
    Test(file="test/test024.c", goal="dereference pointer"),
    Test(file="test/test025.c", goal="dereference pointer"),
    Test(file="test/test026.c", goal="break and continue in loops, modulo operator"),
    Test(file="test/test027.c", goal="unnamed symbol declarations, type declarations, type definitions"),
    Test(file="test/test028.c", goal="label and goto statement"),
    Test(file="test/test029.c", goal="unnamed function arguments, const type qualifiers, static and thread_local storage class specifier"),

    Test(file="test/test030.c", goal="block statement"),
    Test(file="test/test031.c", goal="typedef"),
    Test(file="test/test032.c", goal="dereference operator at start of statement"),
    Test(file="test/test033.c", goal="static array length qualifier in function argument"),
    Test(file="test/test034.c", goal="empty if and while blocks"),
    Test(file="test/test035.c", goal="single unnamed function argument void"),
    Test(file="test/test036.c", goal="vararg function arguments"),
    Test(file="test/test037.c", goal="type as value"),
    Test(file="test/test038.c", goal="escape sequence"),
    Test(file="test/test039.c", goal="conditional (ternary) operator"),
]

# run all tests
for test in tests:
    test.run()