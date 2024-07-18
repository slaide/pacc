import sys, os, io
from dataclasses import dataclass, field
import typing as tp
from enum import Enum

from libbuild import RED,GREEN,BLUE,RESET,LIGHT_GRAY,ORANGE

def fatal(message:str="",exit_code:int=-1):
    sys.stdout.write(f"fatal{' :' if (len(message)>0) else ''} {message}\n")
    sys.stdout.flush()

    sys.exit(exit_code)

@dataclass
class SourceLocation:
    filename:str
    line:int
    col:int

    def __str__(self):
        return f"{self.filename}:{self.line+1}:{self.col+1}"

    def with_col(self,col_override:int)->"SourceLocation":
        return SourceLocation(self.filename,self.line,col_override)

class TokenType(int,Enum):
    WHITESPACE=1
    COMMENT=2
    SYMBOL=3

    GLOBAL_HEADER=4 # local header is just a string

    LITERAL_STRING=0x10
    LITERAL_CHAR=0x11
    LITERAL_NUMBER=0x12

@dataclass
class Token:
    s:str
    src_loc:SourceLocation
    token_type:TokenType=TokenType.SYMBOL
    log_loc:SourceLocation=field(default_factory=lambda:SourceLocation("",-1,-1))

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


def print_tokens(
    tokens:tp.List[Token],
    mode:tp.Literal["source","logical"]="source",
    filename="",
    ignore_comments:bool=False,
    ignore_whitespace:bool=False,
    pad_string:bool=False
):
    # print the tokens, with alternating text color
    token_index=0
    last_loc=SourceLocation("",-1,0)

    line_num=0
    for token in tokens:
        line_num=token.src_loc.line

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
            if line_num>last_loc.line:
                last_loc.col=0
                last_loc.line=line_num
                print(f"\n{filename}:{last_loc.line+1} ",end="")

        if mode=="source":
            padding_size=token.src_loc.col-last_loc.col
        elif mode=="logical":
            padding_size=token.log_loc.col-last_loc.col
        if padding_size>0:
            print(" "*padding_size,end="")

        if token.token_type==TokenType.COMMENT:
            color=LIGHT_GRAY
        elif token.token_type==TokenType.LITERAL_CHAR or token.token_type==TokenType.LITERAL_STRING:
            color=ORANGE

        if pad_string:
            PAD_STR_CHAR="-"
        else:
            PAD_STR_CHAR=""

        print(f"{PAD_STR_CHAR}{color}{token.s}{RESET}{PAD_STR_CHAR}",end="")

        if mode=="source":
            last_loc.col=token.src_loc.col+len(token.s)
        elif mode=="logical":
            last_loc.col=token.log_loc.col+len(token.s)

    print("") # newline

WHITESPACE_NONEWLINE_CHARS=set([whitespace_char for whitespace_char in " \t"])
WHITESPACE_NEWLINE_CHARS=set([whitespace_char for whitespace_char in "\r\n"]+list(WHITESPACE_NONEWLINE_CHARS))
def is_whitespace(c,newline_allowed:bool=True):
    if newline_allowed:
        return c in WHITESPACE_NEWLINE_CHARS
    else:
        return c in WHITESPACE_NONEWLINE_CHARS

SPECIAL_CHARS_SET=set([special_char for special_char in "(){}[]<>,.+-/*&|%^;:=?!'@"])
def is_special(c):
    return c in SPECIAL_CHARS_SET

NUMERIC_CHARS_SET=set([n for n in "0123456789"])
def is_numeric(c):
    return c in NUMERIC_CHARS_SET

def main():
    if len(sys.argv)<2:
        print("no input file")
        return

    filename=sys.argv[1]
    code_file=io.open(filename)

    class Tokenizer:
        " utility class to convert file characters into tokens "

        def __init__(self,filename:str,file_contents:str):
            self.filename=filename

            self.file_contents=file_contents
            self.file_index=0

            self.line_index:int=0
            self.col_index:int=0

            self.tokens:tp.List[Token]=[]

        @property
        def c(self):
            " return character at current pointer location in line "
            return self.c_fut(0)

        def adv(self):
            " advance current line pointer to next character in line "
            if self.remaining and t.c=="\n":
                self.line_index+=1
                self.col_index=0
            else:
                self.col_index+=1

            self.file_index+=1

            # check for line continuation
            # requires: forward clash followed by whitespace
            if t.nc_rem>=1 and t.c=="\\" and is_whitespace(t.c_fut(1),True):
                self.adv()
                while t.remaining and is_whitespace(t.c,False):
                    self.adv()
                assert t.c=="\n", t.c
                t.adv()

        @property
        def nc_rem(self)->int:
            " return number of characters left in file "
            return len(self.file_contents)-self.file_index-1

        def c_fut(self,n):
            " return character in current line n positions in advance of current pointer "
            return self.file_contents[self.file_index+n]

        def current_loc(self):
            " return source location of current pointer into file "

            return SourceLocation(self.filename,self.line_index,self.col_index)

        @property
        def remaining(self):
            " return True if any characters are remaining in the file "

            return self.nc_rem>=0

        def add_tok(self,token:Token):
            " add token to latest token line "
            self.tokens.append(token)

    t=Tokenizer(filename,code_file.read())

    def compound_symbol_present(c_sym:str,current_token:Token)->bool:
        if t.c==c_sym[0] and t.nc_rem>=(len(c_sym)-1):
            ret_s=t.c
            for i in range(1,len(c_sym)):
                c=t.c_fut(i)
                if c!=c_sym[i]:
                    return False
                ret_s+=c
                t.adv()

            current_token.s=ret_s

            return True
        return False

    def parse_terminated_literal(start_char:str,end_char:str,current_token:Token)->bool:
        if len(current_token.s)>0:
            return False

        if t.c==start_char:
            if len(current_token.s)>0:
                return True

            current_token.token_type=TokenType.LITERAL_CHAR

            current_token.s+=t.c
            t.adv()

            while t.remaining:
                # handle escape sequences
                if t.c=="\\":

                    t.adv()
                    if t.c=="n":
                        current_token.s+="\n"
                        t.adv()
                        continue
                    else:
                        fatal(f"unimplemented escape sequence '\\{t.c}'")

                    t.adv()

                    continue

                if t.c=="\n":
                    fatal(f"missing terminating {end_char} at {t.current_loc()}")

                current_token.s+=t.c

                if t.c==end_char:
                    t.adv()
                    break

                t.adv()

            return True

        return False

    # tokenize the file (phase 3, combined with phase 2)
    current_token: tp.Optional[Token]=None
    while t.remaining:
        current_token=Token("",t.current_loc())

        skip_col_increment:bool=False

        # append characters to current token
        while t.remaining:
            if is_whitespace(t.c):
                if current_token.is_empty:
                    current_token.token_type=TokenType.WHITESPACE

                if current_token.is_whitespace:
                    current_token.s+=t.c
                    t.adv()
                    continue

                break

            if current_token.token_type==TokenType.WHITESPACE:
                skip_col_increment=True
                break

            # parse a numeric literal
            char_is_leading_numeric=len(current_token.s)==0 and is_numeric(t.c)
            char_is_dot_followed_by_numeric=t.c=="." and t.nc_rem>0 and is_numeric(t.c_fut(1))
            if char_is_leading_numeric or char_is_dot_followed_by_numeric:
                # leading numeric is legal trailing symbol char, thus we ignore those here
                if char_is_dot_followed_by_numeric and len(current_token.s)>0:
                    skip_col_increment = t.c=="."
                    break

                current_token.token_type=TokenType.LITERAL_NUMBER

                parsed_dot=False
                parsed_exponent=False
                num_exponent_digits=0
                parsed_exponent_sign=False

                while t.remaining:
                    if is_numeric(t.c):
                        current_token.s+=t.c
                        t.adv()

                        if parsed_exponent:
                            num_exponent_digits+=1

                    elif (t.c=="-" or t.c=="+") and parsed_exponent:
                        if parsed_exponent_sign:
                            fatal(f"already parsed exponent sign at {t.current_loc()}")

                        parsed_exponent_sign=True

                        current_token.s+=t.c
                        t.adv()

                        num_exponent_digits+=1

                    elif t.c==".":
                        if parsed_dot:
                            fatal(f"dot already parsed in float literal at {t.current_loc()}")

                        parsed_dot=True

                        current_token.s+=t.c
                        t.adv()

                        if parsed_exponent:
                            num_exponent_digits+=1

                    elif t.c=="e" or t.c=="E":
                        if parsed_exponent:
                            fatal(f"exponent already parsed in float literal at {t.current_loc()}")

                        parsed_exponent=True

                        current_token.s+=t.c
                        t.adv()

                    elif t.c=="'":
                        if not (t.nc_rem>0 and is_numeric(t.c_fut(1))):
                            fatal(f"digit separator cannot appear at end of digit sequence at {t.current_loc()}")

                        if parsed_exponent and num_exponent_digits==0:
                            fatal(f"digit separator cannot appear at start of digit sequence at {t.current_loc()}")

                        current_token.s+=t.c
                        t.adv()

                    else:
                        # append suffix until special char or whitespace
                        while t.remaining and not (is_special(t.c) or is_whitespace(t.c)):
                            current_token.s+=t.c
                            t.adv()

                        skip_col_increment=True
                        break

                if parsed_exponent:
                    if num_exponent_digits==0:
                        fatal(f"exponent has no digits at {t.current_loc()}")

                break

            # parse a character literal
            if parse_terminated_literal("'","'",current_token):
                skip_col_increment=True
                break

            # parse a string literal
            if parse_terminated_literal('"','"',current_token):
                skip_col_increment=True
                break

            # parse a special symbol
            if is_special(t.c):
                if len(current_token.s)>0:
                    skip_col_increment=True
                    break

                # check for single line comment
                if t.c=="/" and t.nc_rem>0 and t.c_fut(1)=="/":
                    current_token.token_type=TokenType.COMMENT

                    while t.remaining:
                        if t.c=="\n":
                            break
                        current_token.s+=t.c
                        t.adv()

                    break

                # check for multi line comment
                if t.c=="/" and t.nc_rem>0 and t.c_fut(1)=="*":
                    current_token.token_type=TokenType.COMMENT

                    current_token.s+=t.c
                    t.adv()

                    while t.remaining:
                        current_token.s+=t.c

                        if len(current_token.s)>=4:
                            if current_token.s[-2:]=="*/":
                                break

                        t.adv()

                    break

                # check for compound symbols (made up from more than one special symbol)

                if compound_symbol_present("--",current_token):
                    break
                if compound_symbol_present("-=",current_token):
                    break
                if compound_symbol_present("++",current_token):
                    break
                if compound_symbol_present("+=",current_token):
                    break
                if compound_symbol_present("||",current_token):
                    break
                if compound_symbol_present("|=",current_token):
                    break
                if compound_symbol_present("&&",current_token):
                    break
                if compound_symbol_present("&=",current_token):
                    break

                if compound_symbol_present("^=",current_token):
                    break

                if compound_symbol_present("!=",current_token):
                    break
                if compound_symbol_present("==",current_token):
                    break

                if compound_symbol_present("<=",current_token):
                    break
                if compound_symbol_present(">=",current_token):
                    break

                if compound_symbol_present("<<",current_token):
                    break
                if compound_symbol_present(">>",current_token):
                    break
                if compound_symbol_present("<<=",current_token):
                    break
                if compound_symbol_present(">>=",current_token):
                    break

                if compound_symbol_present("->",current_token):
                    break

                current_token.s=t.c
                break

            current_token.s+=t.c
            t.adv()

        if current_token is not None and len(current_token.s)>0:
            t.add_tok(current_token)
            current_token=None

        if not skip_col_increment:
            t.adv()

    if current_token is not None and len(current_token.s)!=0:
        t.add_tok(current_token)

    # visually inspect results
    #print_tokens(t.tokens,ignore_comments=False,ignore_whitespace=True,pad_string=False)

    # remove whitespace and comment tokens
    def filter_token(token:Token)->bool:
        match token.token_type:
            case TokenType.COMMENT:
                return True
            case TokenType.WHITESPACE:
                return True
            case tok:
                return False

    final_tokens=[t for t in t.tokens if not filter_token(t)]
    print(f"parsed into {len(final_tokens)} tokens")

    return


if __name__=="__main__":
    main()