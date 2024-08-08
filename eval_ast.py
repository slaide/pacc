import sys, io, time
from dataclasses import dataclass, field
import typing as tp
from enum import Enum
from pathlib import Path
import inspect
import re

import time

from py_util import *

from py_tokenizer import *
from py_preprocessor import *
from py_ast import *

def parse_file(filename:str|None=None,print_results:bool=False):
    if filename is None:
        if len(sys.argv)<2:
            print("no input file")
            return

        filename=sys.argv[1]

    start_time=time.perf_counter()
    t=Tokenizer(filename)
    tokens=t.parse_tokens()
    time0=time.perf_counter()-start_time

    # visually inspect results
    #print_tokens(tokens,mode="logical",ignore_comments=False,ignore_whitespace=True,pad_string=False)

    start_time=time.perf_counter()

    token_lines=sort_tokens_into_lines(tokens)

    time1=time.perf_counter()-start_time
    start_time=time.perf_counter()

    # execute preprocessor (phase 4)

    p=Preprocessor()
    time6=time.perf_counter()-start_time
    start_time=time.perf_counter()
    p.add_lines(token_lines)
    time7=time.perf_counter()-start_time
    start_time=time.perf_counter()
    preprocessed_lines=p.run()

    time2=time.perf_counter()-start_time
    start_time=time.perf_counter()

    # skip over phase 5 for now (handle character sets)

    # phase 6 - concat adjacent string literals

    linear_tokens:list[Token]=[]
    for line in preprocessed_lines:
        linear_tokens.extend(line)

    tokens:list[Token]=[]
    for tok in linear_tokens:
        if len(tokens)>1:
            if tok.token_type==TokenType.LITERAL_STRING and tokens[-1].token_type==TokenType.LITERAL_STRING:
                tokens[-1].s=tokens[-1].s[:-1]+tok.s[1:]
                continue

        tokens.append(tok)

    time3=time.perf_counter()-start_time
    start_time=time.perf_counter()

    # phase 7 - syntactic and semantic analysis, code generation

    if print_results:
        print("tokens going into phase 7:")
        for tok in tokens:
            print(f"{tok.s}",end=" ")
        print("")

    ast=Ast(tokens)

    time4=time.perf_counter()-start_time
    start_time=time.perf_counter()

    if print_results:
        print(f"{GREEN}ast:{RESET}")
        ast.block.print(0)

    time5=time.perf_counter()-start_time
    start_time=time.perf_counter()

    # phase 8 - linking
    # TODO

    return time0,time1,time2,time3,time4,time5,time6,time7

def main():
    start_time=time.perf_counter()

    test_files=Path("test").glob("test*.c")
    test_files=sorted(test_files)

    times=[0]*8

    for f_path in test_files[:64]:
        f=str(f_path)

        print(f"{ORANGE}{f}{RESET}")
        ret=parse_file(f,print_results=False)
        for i,v in enumerate(list(ret)):
            times[i]+=v

        print(times,flush=True)

    end_time=time.perf_counter()

    print(f"ran in {(end_time-start_time):.4f}s")

if __name__=="__main__":

    import cProfile
        
    with cProfile.Profile() as pr:
        main()

        pr.print_stats(sort="tottime")
