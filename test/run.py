#!/usr/bin/env python3

from dataclasses import dataclass
import subprocess as sp
import typing as tp
from pathlib import Path
import os, sys
from tqdm import tqdm
import shlex
import threading

# ensure project root directory
os.chdir(Path(__file__).parent.parent)

BOLD="\033[1m"
RESET="\033[0m"
RED="\033[31m"
GREEN="\033[32m"
ORANGE="\033[33m"
BLUE="\033[34m"
PURPLE="\033[35m"
CYAN="\033[36m"
LIGHT_GRAY="\033[37m"

from enum import Enum
class TestResult(str,Enum):
    SUCCESS="SUCCESS"
    FAILURE="FAILURE"
    TIMEOUT="TIMEOUT"

NUM_TEST_FAILURES_TO_PRINT=2
NUM_TEST_FAILURES_SO_FAR=0

@dataclass
class Test:
    file: str

    goal: tp.Optional[str] = None
    should_fail: bool = False

    flags:tp.Optional[str]=None

    result:tp.Optional[TestResult]=None

    def copy(self)->"Test":
        return Test(
            file=self.file,
            goal=self.goal,
            should_fail=self.should_fail,
            flags=self.flags,
            result=self.result
        )

    def run(self,print_info:bool=True,timeout:float=0.5):
        if print_info:
            print(f"{BOLD}Running test: '{self.file}'{RESET}")

        command=f"bin/main {self.flags or ''} {self.file}"
        cmd_timed_out=False
        run_with_valgrind=False
        while True:
            myenv=os.environ.copy()
            myenv["ASAN_OPTIONS"]="detect_leaks=0" # run with ASAN_OPTIONS=detect_leaks=0 # run with 
            myenv["MallocNanoZone"]="0" # macos specific https://stackoverflow.com/questions/64126942/malloc-nano-zone-abandoned-due-to-inability-to-preallocate-reserved-vm-space
            if run_with_valgrind:
                #command=f"valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes {command}"
                pass

            popen_kwargs={
                "stdout":sp.PIPE,
                "stderr":sp.PIPE
            }

            # read stdout and stderr from pipe into buffer
            stdout_buffer=[]
            stderr_buffer=[]

            def read_stream(stream, storage):
                while True:
                    data = stream.read(1024)
                    if not data:
                        break
                    storage.append(data.decode())
                stream.close()

            # exec command
            proc=sp.Popen(shlex.split(command), env=myenv, **popen_kwargs) # do not use shell=True because killing will not work

            stdout_thread = threading.Thread(target=read_stream, args=(proc.stdout, stdout_buffer))
            stderr_thread = threading.Thread(target=read_stream, args=(proc.stderr, stderr_buffer))
            
            stdout_thread.start()
            stderr_thread.start()

            try:
                proc.wait(timeout=timeout)
            except sp.TimeoutExpired:
                proc.kill() # make sure the timed out process is terminated
                cmd_timed_out=True
            except Error as e:
                proc.kill()
                print(f"exception in command:\n{e}")
            
            if cmd_timed_out:
                if not run_with_valgrind:
                    run_with_valgrind=True
                    continue

                self.result=TestResult.TIMEOUT

            stdout_thread.join()
            stderr_thread.join()

            break

        did_fail=proc.returncode!=0

        test_succeeded=((not did_fail and not self.should_fail) or (did_fail and self.should_fail)) and not cmd_timed_out
        if test_succeeded:
            if print_info:
                print(f"{BOLD}{GREEN}Success: '{self.file}'{RESET}")

            self.result=TestResult.SUCCESS
            return
        else:
            global NUM_TEST_FAILURES_SO_FAR
            NUM_TEST_FAILURES_SO_FAR+=1
            if NUM_TEST_FAILURES_SO_FAR<=NUM_TEST_FAILURES_TO_PRINT:
                def print_info():
                    print(f"{BOLD}{RED}Error: test '{self.file}' failed{RESET}")
                    print(f"$ {command}")
                    print(f"Goal: {self.goal}")
                    if cmd_timed_out:
                        print(f"timed out after {timeout:.2f}s")
                    else:
                        if self.should_fail:
                            print(f"Expected to fail, but succeeded")
                        else:
                            print(f"Expected to succeed, but failed with code {proc.returncode}")

                assert proc.stdout is not None
                assert proc.stderr is not None

                stdout_txt = ''.join(stdout_buffer)
                stderr_txt = ''.join(stderr_buffer)

                # print info once before stdout/stderr
                print_info()

                if len(stdout_txt)>0:
                    print("stdout: ---- \n",stdout_txt)
                else:
                    print("stdout: [empty]")
                if len(stderr_txt)>0:
                    print("stderr: ---- \n",stderr_txt)
                else:
                    print("stderr: [empty]")

                # print again after stdout/stderr
                print_info()
            
        self.result=TestResult.FAILURE

TEST_FILES=[
    Test(file="test/test001.c", goal="function definition without arguments, empty body"),
    Test(file="test/test002.c", goal="function definition with arguments, empty body"),
    Test(file="test/test003.c", goal="function definition with statements in body, single line comment before any code"),
    Test(file="test/test004.c", goal="multi line comment"),
    Test(file="test/test005.c", goal="string literal, local symbol declaration and initialization, return statement"),
    Test(file="test/test006.c", goal="char type symbol declaration and character literals"),
    Test(file="test/test007.c", goal="for loop and function call"),
    Test(file="test/test008.c", goal="preprocessor define directive and macro expansion"),
    Test(file="test/test009.c", goal="preprocessor function-like define directive"),

    Test(file="test/test010.c", goal="include directive with angle brackets, arrow operator, dot operator, call vararg function, struct definition"),
    Test(file="test/test011.c", goal="float literals and casting operation, prefix and postfix operators"),
    Test(file="test/test012.c", goal="array initializer, struct initializer, addrof operator, array subscript operator, explicit struct typename"),
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
    Test(file="test/test038.c", goal="escape sequence in string"),
    Test(file="test/test039.c", goal="conditional (ternary) operator"),

    Test(file="test/test040.c", goal="preprocessor if directive"),
    Test(file="test/test041.c", goal="preprocessor if/else directive"),
    Test(file="test/test042.c", goal="preprocessor nested if directive"),
    Test(file="test/test043.c", goal="preprocessor elif directive"),
    Test(file="test/test044.c", goal="preprocessor ifdef directive, evaluating to true"),
    Test(file="test/test045.c", goal="preprocessor ifdef directive, evaluating to false"),
    Test(file="test/test046.c", goal="preprocessor undef directive"),
    Test(file="test/test047.c", goal="preprocessor include directive"),
    Test(file="test/test048.c", goal="preprocessor pragma once directive"),
    Test(file="test/test049.c", goal="preprocessor nested if directive (2)"),

    Test(file="test/test050.c", goal="preprocessor nested if directive (3)"),
    Test(file="test/test051.c", goal="preprocessor nested if directive (4)"),
    Test(file="test/test052.c", goal="preprocessor value operators"),
    Test(file="test/test053.c", goal="stringification preprcessor operator"),
    Test(file="test/test054.c", goal="internal preprocessor expansion"),
    Test(file="test/test055.c", goal="internal resursive preprocessor expansion"),

    Test(file="test/test056.c", goal="compound symbol definitions"),
    Test(file="test/test057.c", goal="compound symbol definitions with initializer"),
    Test(file="test/test058.c", goal="signed char type use"),
    Test(file="test/test059.c", goal=""),

    Test(file="test/test060.c", goal=""),
    Test(file="test/test061.c", goal="adjacent string literal concatenation (phase 6)"),
    Test(file="test/test062.c", goal="adjacent string literal concatenation (phase 6)"),
    Test(file="test/test063.c", goal="preprocessor __VA_ARGS__ expansion"),
]

def set_flags(t:Test,flags:str)->Test:
    t.flags=flags
    return t

tests=[
    *TEST_FILES,
    *[set_flags(t.copy(),"-p -Imusl/include") for t in TEST_FILES],
    *[set_flags(t.copy(),"-p -Imusl/include -a") for t in TEST_FILES],
]

print(f"{BOLD}running tests...{RESET}")

# run all tests
results={res:0 for res in TestResult}
for test in tqdm(tests):
    test.run(print_info=False,timeout=1.0)
    assert test.result is not None
    results[test.result]+=1

num_total=len(tests)

num_succeeded=results[TestResult.SUCCESS]
num_failed=results[TestResult.FAILURE]
num_timed_out=results[TestResult.TIMEOUT]

perc_success=num_succeeded/num_total*100
perc_failed=num_failed/num_total*100
perc_timed_out=num_timed_out/num_total*100

print(f"{BOLD}Test Results:{RESET}")
print(f"Total: {num_total}")
max_num_tests_len=max(len(str(n)) for n in [num_succeeded,num_failed,num_timed_out])
def pad_to(s,n:int,fmt_str:str="{s}")->str:
    s_str=fmt_str.format(s=s)
    return (" "*(n-len(s_str)))+s_str
num_succeeded_str=pad_to(num_succeeded,max_num_tests_len)
num_failed_str=pad_to(num_failed,max_num_tests_len)
num_timed_out_str=pad_to(num_timed_out,max_num_tests_len)
perc_success_str=pad_to(perc_success,6,fmt_str="{s:.2f}")
perc_failed_str=pad_to(perc_failed,6,fmt_str="{s:.2f}")
perc_timed_out_str=pad_to(perc_timed_out,6,fmt_str="{s:.2f}")
print(f"{GREEN  }Succeeded : {num_succeeded_str} ({perc_success_str  } %){RESET}")
print(f"{RED    }Failed    : {num_failed_str   } ({perc_failed_str   } %){RESET}")
print(f"{ORANGE }Timed out : {num_timed_out_str} ({perc_timed_out_str} %){RESET}")
