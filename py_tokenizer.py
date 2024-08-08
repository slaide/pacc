import sys, io, time
from dataclasses import dataclass, field
import typing as tp
from enum import Enum
from pathlib import Path
import inspect
import re

from py_util import *

@dataclass
class SourceLocation:
    filename:str
    line:int
    col:int

    @tp.override
    def __str__(self):
        return f"{self.filename}:{self.line+1}:{self.col+1}"

    def with_col(self,col_override:int)->"SourceLocation":
        return SourceLocation(self.filename,self.line,col_override)

    @staticmethod
    def placeholder()->"SourceLocation":
        return SourceLocation("",0,0)
    @staticmethod
    def invalid()->"SourceLocation":
        return SourceLocation("",-1,-1)

class TokenType(int,Enum):
    WHITESPACE=1
    COMMENT=2

    SYMBOL=3
    OPERATOR_PUNCTUATION=4

    KEYWORD=6

    GLOBAL_HEADER=9 # local header is just a LITERAL_STRING

    LITERAL_STRING=0x10
    LITERAL_CHAR=0x11
    LITERAL_NUMBER=0x12

@dataclass
class Token:
    s:str
    src_loc:SourceLocation
    token_type:TokenType=TokenType.SYMBOL
    log_loc:SourceLocation=field(default_factory=lambda:SourceLocation.invalid())

    expanded_from_macros:list[str]|None=None

    def copy(self)->"Token":
        return Token(
            s=self.s,
            src_loc=self.src_loc,
            token_type=self.token_type,
            log_loc=self.log_loc,
            expanded_from_macros=[i for i in self.expanded_from_macros] if self.expanded_from_macros is not None else None
        )

    def expand_from(self,macro_name:str):
        if self.expanded_from_macros is None:
            self.expanded_from_macros=[]

        self.expanded_from_macros.append(macro_name)

    def is_expanded_from(self,macro_name:str):
        if self.expanded_from_macros is None:
            return False

        for expanded_from_macro_name in self.expanded_from_macros:
            if expanded_from_macro_name==macro_name:
                return True

        return False

    @property
    def is_whitespace(self)->bool:
        if self.token_type!=TokenType.WHITESPACE:
            return False

        for c in self.s:
            if not is_whitespace(c):
                return False

        return True

    @property
    def is_empty(self):
        return len(self.s)==0

    def is_valid_symbol(self)->bool:
        if not re.match("[A-Za-z_][A-Za-z_0-9]*",self.s):
            return False

        return True


def print_tokens(
    tokens:list[Token],
    mode:tp.Literal["source","logical"]="source",
    filename:str="",
    ignore_comments:bool=False,
    ignore_whitespace:bool=False,
    pad_string:bool=False
):
    # print the tokens, with alternating text color
    token_index=0
    last_loc=SourceLocation("",-1,0)

    for token in tokens:
        if token.token_type==TokenType.COMMENT and ignore_comments:
            continue
        elif token.token_type==TokenType.WHITESPACE and ignore_whitespace:
            continue

        if last_loc.filename!=token.src_loc.filename:
            last_loc.filename=token.src_loc.filename
            print(f"file {token.src_loc.filename}:")

        token_index+=1
        color=GREEN if token_index%2==0 else RED

        if mode=="source":
            if token.src_loc.line>last_loc.line:
                last_loc.col=0
                last_loc.line=token.src_loc.line
                print(f"\n{filename}:{last_loc.line+1} ",end="")

        elif mode=="logical":
            if token.log_loc.line>last_loc.line:
                last_loc.col=0
                last_loc.line=token.log_loc.line
                print(f"\n{filename}:{last_loc.line+1} ",end="")

        if mode=="source":
            padding_size=token.src_loc.col-last_loc.col
        elif mode=="logical":
            padding_size=token.log_loc.col-last_loc.col
        if padding_size>0:
            print(ind(padding_size),end="")

        if token.token_type==TokenType.COMMENT:
            color=LIGHT_GRAY
        elif token.token_type==TokenType.LITERAL_CHAR or token.token_type==TokenType.LITERAL_STRING:
            color=ORANGE

        if pad_string:
            pad_str_char="-"
        else:
            pad_str_char=""

        print(f"{pad_str_char}{color}{token.s}{RESET}{pad_str_char}",end="")

        if mode=="source":
            last_loc.col=token.src_loc.col+len(token.s)
        elif mode=="logical":
            last_loc.col=token.log_loc.col+len(token.s)

    print("") # newline


WHITESPACE_NONEWLINE_CHARS=set([whitespace_char for whitespace_char in " \t"])
WHITESPACE_NEWLINE_CHARS=set([whitespace_char for whitespace_char in "\r\n"]+list(WHITESPACE_NONEWLINE_CHARS))
def is_whitespace(c:str,newline_allowed:bool=True):
    assert len(c)==1, f"{len(c) = } ; {c = }"
    if newline_allowed:
        return c in WHITESPACE_NEWLINE_CHARS
    else:
        return c in WHITESPACE_NONEWLINE_CHARS

SPECIAL_CHARS_SET=set([special_char for special_char in "(){}[]<>,.+-/*&|%^;:=?!\"'@#~"])
def is_special(c:str):
    assert len(c)==1, f"{len(c) = } ; {c = }"
    return c in SPECIAL_CHARS_SET

NUMERIC_CHARS_SET=set([n for n in "0123456789"])
def is_numeric(c:str):
    assert len(c)==1, f"{len(c) = } ; {c = }"
    return c in NUMERIC_CHARS_SET


SPECIAL_COMPOUND_SYMBOLS=[
    "->",

    "++",
    "--",

    "||",
    "&&",

    "==",
    "!=",

    "<=",
    ">=",

    "-=",
    "+=",
    "|=",
    "&=",
    "^=",

    "<<",
    ">>",

    "<<=",
    ">>=",

    "...",
]
"special compound symbols, e.g. arrow operator ->"
SPECIAL_COMPOUND_SYMBOLS_CHAR_AT_ZERO=set((s[0] for s in SPECIAL_COMPOUND_SYMBOLS))
"set to allow quick checking if any compound symbol may be matched"

class Tokenizer:
    " utility class to convert file characters into tokens "

    def __init__(self,filename:str):
        self.filename:str=filename

        file=io.open(filename)
        self.file_contents:str=file.read()
        file.close()
        self.file_index:int=0

        self.logical_line_index:int=0
        self.logical_col_index:int=0
        self.line_index:int=0
        self.col_index:int=0

        self.tokens:list[Token]=[]

    @property
    def c(self)->str:
        " return character at current pointer location in line "
        return self.c_fut(0)

    def adv(self,logical_line_adjust:bool=True):
        " advance current line pointer to next character in line "
        if self.remaining and self.c=="\n":
            self.line_index+=1
            if logical_line_adjust:
                self.logical_line_index+=1
                self.logical_col_index=0
            self.col_index=0
        else:
            self.logical_col_index+=1
            self.col_index+=1

        self.file_index+=1

        # check for line continuation
        # requires: forward clash followed by whitespace
        if self.nc_rem>=1 and self.c=="\\" and is_whitespace(self.c_fut(1),True):
            # line continuation character does not exist in the logical source code
            self.logical_col_index-=1

            self.adv(logical_line_adjust=False)
            while self.remaining and is_whitespace(self.c,False):
                self.adv()

            assert self.c=="\n", self.c
            self.adv(logical_line_adjust=False)

    @property
    def nc_rem(self)->int:
        " return number of characters left in file "
        return len(self.file_contents)-self.file_index-1

    def c_fut(self,n:int)->str:
        " return character in current line n positions in advance of current pointer "
        return self.file_contents[self.file_index+n]

    def current_loc(self)->"SourceLocation":
        " return source location of current pointer into file "

        return SourceLocation(self.filename,self.line_index,self.col_index)

    def current_log_loc(self)->"SourceLocation":
        return SourceLocation(self.filename,self.logical_line_index,self.logical_col_index)

    @property
    def remaining(self):
        " return True if any characters are remaining in the file "

        return self.nc_rem>=0

    def add_tok(self,token:Token):
        " add token to latest token line "
        self.tokens.append(token)

    def compound_symbol_present(self,c_sym:str,current_token:"Token")->bool:
        if self.c==c_sym[0] and self.nc_rem>=(len(c_sym)-1):
            for i in range(1,len(c_sym)):
                c=self.c_fut(i)
                if c!=c_sym[i]:
                    return False

            # only advance if symbol has been found
            for i in range(1,len(c_sym)):
                self.adv()

            current_token.s=c_sym
            current_token.token_type=TokenType.OPERATOR_PUNCTUATION

            return True

        return False

    def parse_terminated_literal(self,start_char:str,end_char:str,current_token:Token,token_type:TokenType)->bool:
        if len(current_token.s)>0:
            return False

        if self.c==start_char:
            if len(current_token.s)>0:
                current_token.token_type=token_type
                return True

            current_token.token_type=TokenType.LITERAL_CHAR

            current_token.s+=self.c
            self.adv()

            while self.remaining:
                # handle escape sequences
                if self.c=="\\":

                    self.adv()

                    match self.c:
                        case "n":
                            current_token.s+="\n"
                        case "\"":
                            current_token.s+="\""
                        case "\\":
                            current_token.s+="\\"
                        case "'":
                            current_token.s+="'"
                        case _:
                            if is_numeric(self.c):
                                match self.c:
                                    case "0":
                                        current_token.s+="\0"
                                    case other:
                                        fatal(f"unimplemented {other}")
                            else:
                                fatal(f"unimplemented escape sequence '\\{self.c}'")

                    self.adv()

                    continue

                elif self.c=="\n":
                    fatal(f"missing terminating {end_char} at {self.current_loc()}")

                current_token.s+=self.c

                if self.c==end_char:
                    self.adv()
                    break

                self.adv()

            current_token.token_type=token_type
            return True

        return False

    def parse_tokens(self):
        self.tokens=[]

        # tokenize the file (phase 3, combined with phase 2)
        current_token:Token|None=None
        while self.remaining:
            current_token=Token("",src_loc=self.current_loc(),log_loc=self.current_log_loc())

            skip_col_increment:bool=False

            # append characters to current token
            while self.remaining:
                if is_whitespace(self.c):
                    if current_token.is_empty:
                        current_token.token_type=TokenType.WHITESPACE

                    if current_token.is_whitespace:
                        current_token.s+=self.c
                        self.adv()
                        continue

                    break

                elif current_token.token_type==TokenType.WHITESPACE:
                    skip_col_increment=True
                    break

                # parse a numeric literal
                char_is_leading_numeric=len(current_token.s)==0 and is_numeric(self.c)
                char_is_dot_followed_by_numeric=self.c=="." and self.nc_rem>0 and is_numeric(self.c_fut(1))
                if char_is_leading_numeric or char_is_dot_followed_by_numeric:
                    # leading numeric is legal trailing symbol char, thus we ignore those here
                    if char_is_dot_followed_by_numeric and len(current_token.s)>0:
                        skip_col_increment = self.c=="."
                        break

                    current_token.token_type=TokenType.LITERAL_NUMBER

                    parsed_dot=False
                    parsed_exponent=False
                    num_exponent_digits=0
                    parsed_exponent_sign=False

                    while self.remaining:
                        if is_numeric(self.c):
                            current_token.s+=self.c
                            self.adv()

                            if parsed_exponent:
                                num_exponent_digits+=1

                        elif (self.c=="-" or self.c=="+") and parsed_exponent:
                            if parsed_exponent_sign:
                                fatal(f"already parsed exponent sign at {self.current_loc()}")

                            parsed_exponent_sign=True

                            current_token.s+=self.c
                            self.adv()

                            num_exponent_digits+=1

                        elif self.c==".":
                            if parsed_dot:
                                fatal(f"dot already parsed in float literal at {self.current_loc()}")

                            parsed_dot=True

                            current_token.s+=self.c
                            self.adv()

                            if parsed_exponent:
                                num_exponent_digits+=1

                        elif self.c=="e" or self.c=="E":
                            if parsed_exponent:
                                fatal(f"exponent already parsed in float literal at {self.current_loc()}")

                            parsed_exponent=True

                            current_token.s+=self.c
                            self.adv()

                        elif self.c=="'":
                            if not (self.nc_rem>0 and is_numeric(self.c_fut(1))):
                                fatal(f"digit separator cannot appear at end of digit sequence at {self.current_loc()}")

                            if parsed_exponent and num_exponent_digits==0:
                                fatal(f"digit separator cannot appear at start of digit sequence at {self.current_loc()}")

                            current_token.s+=self.c
                            self.adv()

                        else:
                            # append suffix until special char or whitespace
                            while self.remaining and not (is_special(self.c) or is_whitespace(self.c)):
                                current_token.s+=self.c
                                self.adv()

                            skip_col_increment=True
                            break

                    if parsed_exponent:
                        if num_exponent_digits==0:
                            fatal(f"exponent has no digits at {self.current_loc()}")

                    break

                # parse a character literal
                if self.parse_terminated_literal("'","'",current_token,token_type=TokenType.LITERAL_CHAR):
                    skip_col_increment=True
                    break

                # parse a string literal
                if self.parse_terminated_literal('"','"',current_token,token_type=TokenType.LITERAL_STRING):
                    skip_col_increment=True
                    break

                # parse a special symbol (includes comments)
                if is_special(self.c):
                    if len(current_token.s)>0:
                        skip_col_increment=True
                        break

                    # check for single line comment
                    if self.c=="/" and self.nc_rem>0 and self.c_fut(1)=="/":
                        current_token.token_type=TokenType.COMMENT

                        while self.remaining:
                            if self.c=="\n":
                                break
                                
                            current_token.s+=self.c
                            self.adv()

                        break

                    # check for multi line comment
                    if self.c=="/" and self.nc_rem>0 and self.c_fut(1)=="*":
                        current_token.token_type=TokenType.COMMENT

                        current_token.s+=self.c
                        self.adv()

                        while self.remaining:
                            current_token.s+=self.c

                            if len(current_token.s)>=4:
                                if current_token.s[-2:]=="*/":
                                    break

                            self.adv()

                        break

                    # check for compound symbols (made up from more than one special symbol)

                    if self.c in SPECIAL_COMPOUND_SYMBOLS_CHAR_AT_ZERO:
                        matched_compound_symbol=False
                        for compound_symbol in SPECIAL_COMPOUND_SYMBOLS:
                            if self.compound_symbol_present(compound_symbol,current_token):
                                matched_compound_symbol=True
                                break

                        if matched_compound_symbol:
                            break

                    # if not compound but still special:
                    current_token.token_type=TokenType.OPERATOR_PUNCTUATION
                    current_token.s=self.c
                    break

                current_token.s+=self.c
                self.adv()

            if current_token is not None and len(current_token.s)>0:
                self.add_tok(current_token)
                current_token=None

            if not skip_col_increment:
                self.adv()

        if current_token is not None and len(current_token.s)!=0:
            self.add_tok(current_token)

        return self.tokens
