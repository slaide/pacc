#!/usr/bin/env python3

from dataclasses import dataclass
import subprocess as sp
import typing as tp
from pathlib import Path
import os
from tqdm import tqdm
import shlex

# ensure project root directory
os.chdir(Path(__file__).parent.parent)

BOLD="\033[1m"
RESET="\033[0m"
RED="\033[31m"
GREEN="\033[32m"

from enum import Enum
class TestResult(str,Enum):
    SUCCESS="SUCCESS"
    FAILURE="FAILURE"
    TIMEOUT="TIMEOUT"

@dataclass
class Test:
    file: str

    goal: tp.Optional[str] = None
    should_fail: bool = False

    flags:tp.Optional[str]=None

    result:tp.Optional[TestResult]=None

    def run(self,print_info:bool=True,timeout:float=0.5):
        if print_info:
            print(f"{BOLD}Running test: '{self.file}'{RESET}")

        command=f"bin/main {self.flags or ''} {self.file}"

        # exec command
        proc=sp.Popen(shlex.split(command), stdout=sp.PIPE, stderr=sp.PIPE) # do not use shell=True because killing will not work
        try:
            proc.wait(timeout=timeout)
        except sp.TimeoutExpired:
            proc.kill() # make sure the timed out process is terminated
            print(f"{BOLD}{RED}Error: test '{self.file}' timed out{RESET}")
            self.result=TestResult.TIMEOUT
            return

        did_fail=proc.returncode!=0

        if (not did_fail and not self.should_fail) or (did_fail and self.should_fail):
            if print_info:
                print(f"{BOLD}{GREEN}Success: '{self.file}'{RESET}")

            self.result=TestResult.SUCCESS
            return
        else:
            print(f"{BOLD}{RED}Error: test '{self.file}' failed{RESET}")
            print(f"Goal: {self.goal}")
            if self.should_fail:
                print(f"Expected to fail, but succeeded")
            else:
                print(f"Expected to succeed, but failed with code {proc.returncode}")

        assert proc.stdout is not None
        assert proc.stderr is not None
        print("stdout: ---- \n",proc.stdout.read().decode("utf-8"))
        print("stderr: ---- \n",proc.stderr.read().decode("utf-8"))
            
        self.result=TestResult.FAILURE

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

    Test(file="test/test040.c", goal="preprocessor if directive", flags="-p"),
    Test(file="test/test041.c", goal="preprocessor if/else directive", flags="-p"),
    Test(file="test/test042.c", goal="preprocessor nested if directive", flags="-p"),
    Test(file="test/test043.c", goal="preprocessor elif directive", flags="-p"),
    Test(file="test/test044.c", goal="preprocessor ifdef directive, evaluating to true", flags="-p"),
    Test(file="test/test045.c", goal="preprocessor ifdef directive, evaluating to false", flags="-p"),
    Test(file="test/test046.c", goal="preprocessor undef directive", flags="-p"),
    Test(file="test/test047.c", goal="preprocessor include directive", flags="-p"),
    Test(file="test/test048.c", goal="preprocessor pragma once directive", flags="-p"),
    Test(file="test/test049.c", goal="preprocessor nested if directive (2)", flags="-p"),

    Test(file="test/test050.c", goal="preprocessor nested if directive (3)", flags="-p"),
    Test(file="test/test051.c", goal="preprocessor nested if directive (4)", flags="-p"),
]

# run all tests
results={res:0 for res in TestResult}
for test in tqdm(tests):
    test.run(print_info=False,timeout=0.5)
    assert test.result is not None
    results[test.result]+=1

num_total=len(tests)

num_succeeded=results[TestResult.SUCCESS]
num_failed=results[TestResult.FAILURE]
num_timed_out=results[TestResult.TIMEOUT]

perc_success=num_succeeded/num_total*100
perc_failed=num_failed/num_total*100
perc_timed_out=num_timed_out/num_total*100

print(f"{BOLD}Results:{RESET}")
print(f"Total: {num_total}")
print(f"Succeeded: {num_succeeded} ({perc_success:.2f} %)")
print(f"Failed: {num_failed} ({perc_failed:.2f} %)")
print(f"Timed out: {num_timed_out} ({perc_timed_out:.2f} %)")
