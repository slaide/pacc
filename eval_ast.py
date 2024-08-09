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
            return []

        filename=sys.argv[1]

    t=Tokenizer(filename)
    init_tokens=t.parse_tokens()

    # visually inspect results
    #print_tokens(tokens,mode="logical",ignore_comments=False,ignore_whitespace=True,pad_string=False)

    token_lines=sort_tokens_into_lines(init_tokens)

    # execute preprocessor (phase 4)

    p=Preprocessor()
    p.add_lines(token_lines)
    preprocessed_lines=p.run()

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

    # phase 7 - syntactic and semantic analysis, code generation

    if print_results:
        print("tokens going into phase 7:")
        for tok in tokens:
            print(f"{tok.s}",end=" ")
        print("")

    ast=Ast(tokens)

    if print_results:
        print(f"{GREEN}ast:{RESET}")
        ast.block.print(0)


    # phase 8 - linking
    # TODO

    return

def main():
    total_start_time=time.perf_counter()

    test_files=Path("test").glob("test*.c")
    test_files=sorted(test_files)

    for f_path in test_files[:64]:
        f=str(f_path)

        file_start_time=time.perf_counter()
        print(f"{ORANGE}{f}{RESET}",flush=True,end="")
        parse_file(f,print_results=False)
        file_end_time=time.perf_counter()
        print(f" in {((file_end_time-file_start_time)*1e3):8.3f}ms",flush=True)

    total_end_time=time.perf_counter()

    print(f"ran in {(total_end_time-total_start_time):.4f}s")

if __name__=="__main__":

    do_profile=False

    if not do_profile:
        main()
    else:
        import cProfile
            
        with cProfile.Profile() as pr:
            main()

            pr.print_stats(sort="tottime")
