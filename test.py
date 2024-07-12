#!/usr/bin/env python3

from dataclasses import dataclass
import subprocess as sp
import typing as tp
from pathlib import Path
import os, sys
from tqdm import tqdm
import shlex
import threading
from concurrent import futures as fut

from libbuild import *

argparser=ArgParser("run tests on the pacc compiler")

argparser.add(name="--target",short="-t",help="run specific target",key="target",arg_store_op=ArgStore.store_value,type=str)
argparser.add(name="--num-threads",short="-j",help="number of compilation threads",key="num_threads",arg_store_op=ArgStore.store_value,default=1,type=int)
argparser.add(name="--help",short="-h",help="Prints this help message",key="show_help",arg_store_op=ArgStore.presence_flag)

args=argparser.parse(sys.argv[1:])

if args.get("show_help",False):
    argparser.print_help()
    exit(0)

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

class TestLevel(int,Enum):
    """ compiler stage targeted by the test """
    TOKENIZE=3
    " tokenization may fail e.g. due to unterminated string literals"
    PREPROCESS=4
    " preprocessor may fail e.g. due to unterminated preprocessor directives"
    PARSE=7
    " parsing may fail e.g. due to syntax errors"
    GENERATE=8
    LINK=9
    " linking may fail e.g. due to unresolved symbols"
    EXECUTE=10

@dataclass(unsafe_hash=True,order=True)
class Test:
    file: str
    level:TestLevel

    goal: tp.Optional[str] = None
    should_fail: bool = False

    result:tp.Optional[TestResult]=None

    command:tp.Optional[str]=None
    exit_code:tp.Optional[int]=None

    @property
    def flags(self)->str:
        ret=""
        if self.level>=TestLevel.TOKENIZE:
            ret+=""

        if self.level>=TestLevel.PREPROCESS:
            ret+=" -p -Imusl/include"

        if self.level>=TestLevel.PARSE:
            ret+=" -a"

        return ret

    def copy(self)->"Test":
        return Test(
            file=self.file,
            level=self.level,
            goal=self.goal,
            should_fail=self.should_fail,
            result=self.result
        )

    def run(self,timeout:float=0.5):
        command=f"bin/main {self.flags or ''} {self.file}"
        self.command=command
        cmd_timed_out=False

        myenv=os.environ.copy()
        myenv["ASAN_OPTIONS"]="detect_leaks=0" # disable leak detection, which is not currently in scope of the ongoing code changes
        myenv["MallocNanoZone"]="0" # macos specific https://stackoverflow.com/questions/64126942/malloc-nano-zone-abandoned-due-to-inability-to-preallocate-reserved-vm-space

        popen_kwargs={
            "stdout":sp.PIPE,
            "stderr":sp.PIPE
        }

        # read stdout and stderr from pipe into buffer
        stdout_buffer:tp.List[str]=[]
        stderr_buffer:tp.List[str]=[]

        def read_stream(stream:tp.IO[tp.Any], storage:tp.List[str]):
            while True:
                data = stream.read(1024)
                if not data:
                    break
                storage.append(data.decode())
            stream.close()

        # exec command (no shell to allow proc.kill)
        proc=sp.Popen(shlex.split(command), env=myenv, **popen_kwargs)

        stdout_thread = threading.Thread(target=read_stream, args=(proc.stdout, stdout_buffer))
        stderr_thread = threading.Thread(target=read_stream, args=(proc.stderr, stderr_buffer))
        
        stdout_thread.start()
        stderr_thread.start()

        try:
            proc.wait(timeout=timeout)
        except sp.TimeoutExpired:
            proc.kill() # make sure the timed out process is terminated
            cmd_timed_out=True
        except Exception as e:
            # catch other exceptions to ensure the process is killed
            proc.kill()
            raise RuntimeError(f"exception in command $ {command}:\n{e}")
        
        self.exit_code=proc.returncode
        
        if cmd_timed_out:
            self.result=TestResult.TIMEOUT

        stdout_thread.join()
        stderr_thread.join()

        did_fail=proc.returncode!=0

        test_succeeded=((not did_fail and not self.should_fail) or (did_fail and self.should_fail)) and not cmd_timed_out
        if test_succeeded:
            self.result=TestResult.SUCCESS
            return
            
        if not cmd_timed_out:
            self.result=TestResult.FAILURE

TEST_FILES=[
    Test(file="test/test001.c", level=TestLevel.PARSE, goal="function definition without arguments, empty body"),
    Test(file="test/test002.c", level=TestLevel.PARSE, goal="function definition with arguments, empty body"),
    Test(file="test/test003.c", level=TestLevel.PARSE, goal="function definition with statements in body, single line comment before any code"),
    Test(file="test/test004.c", level=TestLevel.PARSE, goal="multi line comment"),
    Test(file="test/test005.c", level=TestLevel.PARSE, goal="string literal, local symbol declaration and initialization, return statement"),
    Test(file="test/test006.c", level=TestLevel.PARSE, goal="char type symbol declaration and character literals"),
    Test(file="test/test007.c", level=TestLevel.PARSE, goal="for loop and function call"),
    Test(file="test/test008.c", level=TestLevel.PARSE, goal="preprocessor define directive and macro expansion"),
    Test(file="test/test009.c", level=TestLevel.PARSE, goal="preprocessor function-like define directive"),

    Test(file="test/test010.c", level=TestLevel.PARSE, goal="include directive with angle brackets, arrow operator, dot operator, call vararg function, struct definition"),
    Test(file="test/test011.c", level=TestLevel.PARSE, goal="float literals and casting operation, prefix and postfix operators"),
    Test(file="test/test012.c", level=TestLevel.PARSE, goal="array initializer, struct initializer, addrof operator, array subscript operator, explicit struct typename"),
    Test(file="test/test013.c", level=TestLevel.PARSE, goal="explicit struct typename"),
    Test(file="test/test014.c", level=TestLevel.PARSE, goal="const type"),
    Test(file="test/test015.c", level=TestLevel.PARSE, goal="casting expression on struct initializer, typedef"),
    Test(file="test/test016.c", level=TestLevel.PARSE, goal="if statement"),
    Test(file="test/test017.c", level=TestLevel.PARSE, goal="array initializer"),
    Test(file="test/test018.c", level=TestLevel.PARSE, goal="parens wrapped value operations"),
    Test(file="test/test019.c", level=TestLevel.PARSE, goal="if statement with single statement in body, nested logical operators"),

    Test(file="test/test020.c", level=TestLevel.PARSE, goal="if statement, array initializer"),
    Test(file="test/test021.c", level=TestLevel.PARSE, goal="unary logical negation, unary binary negation"),
    Test(file="test/test022.c", level=TestLevel.PARSE, goal="while loop, for loop"),
    Test(file="test/test023.c", level=TestLevel.PARSE, goal="do while loop, multiple versions of a for loop"),
    Test(file="test/test024.c", level=TestLevel.PARSE, goal="dereference pointer"),
    Test(file="test/test025.c", level=TestLevel.PARSE, goal="dereference pointer"),
    Test(file="test/test026.c", level=TestLevel.PARSE, goal="break and continue in loops, modulo operator"),
    Test(file="test/test027.c", level=TestLevel.PARSE, goal="unnamed symbol declarations, type declarations, type definitions"),
    Test(file="test/test028.c", level=TestLevel.PARSE, goal="label and goto statement"),
    Test(file="test/test029.c", level=TestLevel.PARSE, goal="unnamed function arguments, const type qualifiers, static and thread_local storage class specifier"),

    Test(file="test/test030.c", level=TestLevel.PARSE, goal="block statement"),
    Test(file="test/test031.c", level=TestLevel.PARSE, goal="typedef"),
    Test(file="test/test032.c", level=TestLevel.PARSE, goal="dereference operator at start of statement"),
    Test(file="test/test033.c", level=TestLevel.PARSE, goal="static array length qualifier in function argument"),
    Test(file="test/test034.c", level=TestLevel.PARSE, goal="empty if and while blocks"),
    Test(file="test/test035.c", level=TestLevel.PARSE, goal="single unnamed function argument void"),
    Test(file="test/test036.c", level=TestLevel.PARSE, goal="vararg function arguments"),
    Test(file="test/test037.c", level=TestLevel.PARSE, goal="type as value"),
    Test(file="test/test038.c", level=TestLevel.PARSE, goal="escape sequence in string"),
    Test(file="test/test039.c", level=TestLevel.PARSE, goal="conditional (ternary) operator"),

    Test(file="test/test040.c", level=TestLevel.PARSE, goal="preprocessor if directive"),
    Test(file="test/test041.c", level=TestLevel.PARSE, goal="preprocessor if/else directive"),
    Test(file="test/test042.c", level=TestLevel.PARSE, goal="preprocessor nested if directive"),
    Test(file="test/test043.c", level=TestLevel.PARSE, goal="preprocessor elif directive"),
    Test(file="test/test044.c", level=TestLevel.PARSE, goal="preprocessor ifdef directive, evaluating to true"),
    Test(file="test/test045.c", level=TestLevel.PARSE, goal="preprocessor ifdef directive, evaluating to false"),
    Test(file="test/test046.c", level=TestLevel.PARSE, goal="preprocessor undef directive"),
    Test(file="test/test047.c", level=TestLevel.PARSE, goal="preprocessor include directive"),
    Test(file="test/test048.c", level=TestLevel.PARSE, goal="preprocessor pragma once directive"),
    Test(file="test/test049.c", level=TestLevel.PARSE, goal="preprocessor nested if directive (2)"),

    Test(file="test/test050.c", level=TestLevel.PARSE, goal="preprocessor nested if directive (3)"),
    Test(file="test/test051.c", level=TestLevel.PARSE, goal="preprocessor nested if directive (4)"),
    Test(file="test/test052.c", level=TestLevel.PARSE, goal="preprocessor value operators"),
    Test(file="test/test053.c", level=TestLevel.PARSE, goal="stringification preprcessor operator"),
    Test(file="test/test054.c", level=TestLevel.PARSE, goal="internal preprocessor expansion"),
    Test(file="test/test055.c", level=TestLevel.PARSE, goal="internal resursive preprocessor expansion"),

    Test(file="test/test056.c", level=TestLevel.PARSE, goal="compound symbol definitions"),
    Test(file="test/test057.c", level=TestLevel.PARSE, goal="compound symbol definitions with initializer"),
    Test(file="test/test058.c", level=TestLevel.PARSE, goal="signed char type use"),
    Test(file="test/test059.c", level=TestLevel.PARSE, goal="recursive macro expansion"),

    Test(file="test/test060.c", level=TestLevel.PARSE, goal="nested macro expansion"),
    Test(file="test/test061.c", level=TestLevel.PARSE, goal="adjacent string literal concatenation (phase 6)"),
    Test(file="test/test062.c", level=TestLevel.PARSE, goal="adjacent string literal concatenation (phase 6)"),
    Test(file="test/test063.c", level=TestLevel.PARSE, goal="preprocessor __VA_ARGS__ expansion"),

    Test(file="test/test064.c", level=TestLevel.PARSE, goal="function argument has incompatible type", should_fail=True),
    Test(file="test/test065.c", level=TestLevel.PARSE, goal="accessing non-existent field on a struct", should_fail=True),
    Test(file="test/test066.c", level=TestLevel.PARSE, goal="leftover tokens at end of file", should_fail=True),
]

tests=[
    *TEST_FILES,
]

test_target=args.get("target")
if test_target is not None:
    if len(tests)<int(test_target)+1:
        print(f"{RED}error: test target {test_target} out of range{RESET}")
        exit(1)

    tests=[test for i,test in enumerate(tests) if i==int(test_target)]

num_test_workers=args.get("num_threads") or get_num_cores()
if num_test_workers>1:
    threadpool=fut.ThreadPoolExecutor(max_workers=num_test_workers)

print(f"{BOLD}running tests...{RESET}")

failed_tests:tp.List[Test]=[]
timeout_tests:tp.List[Test]=[]

results={res:0 for res in TestResult}

# run all tests
if num_test_workers>1:
    test_future_handles:tp.List[tp.Optional[tp.Tuple[Test,fut.Future]]]=[None for _ in range(len(tests))]
    for i,test in enumerate(tests):
        test_future=threadpool.submit(lambda test:test.run(timeout=1.0),test)
        assert test_future is not None
        assert test not in test_future_handles
        test_future_handles[i]=(test,test_future)

    test_progress=tqdm(total=len(tests),desc="running tests",unit="test")
    while True:
        done=True

        for i,val in enumerate(test_future_handles):
            if val is None:
                continue

            test,test_future=val
            if not test_future.done():
                done=False
                break

            test_future_handles[i]=None
            test_progress.update(1)

        # wait for a short time for tests to run in the background
        time.sleep(5e-3)

        if done:
            break

    test_progress.close()

    threadpool.shutdown(wait=True)

else:
    for test in tqdm(tests):
        test.run(timeout=1.0)

for test in tests:
    assert test.result is not None
    if test.result==TestResult.FAILURE:
        failed_tests.append(test)
    if test.result==TestResult.TIMEOUT:
        timeout_tests.append(test)
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
for test in failed_tests:
    if test.should_fail:
        extra_info=" (expected to fail)"
    else:
        extra_info=f" (expected to succeed; failed with {test.exit_code})"
    print(f"{RED}Failed test: '{test.command}'{RESET} {extra_info}")
print(f"{ORANGE }Timed out : {num_timed_out_str} ({perc_timed_out_str} %){RESET}")
for test in timeout_tests:
    print(f"{ORANGE}Timed out test: '{test.command}'{RESET}")
