import sys, io, time
from dataclasses import dataclass, field
import typing as tp
from enum import Enum
from pathlib import Path
import inspect
import re
import glob

from libbuild import RED,GREEN,RESET,LIGHT_GRAY,ORANGE

def fatal(message:str="",exit_code:int=-1)->tp.NoReturn:
    current_frame=inspect.currentframe()
    assert current_frame is not None

    frames=[current_frame]

    while (current_frame:=current_frame.f_back) is not None:
        frames.append(current_frame)

    # omit bottom of stack (which is this function) and top of stack (which is the module)
    # and reverse to print lowest stack information last
    frames=reversed(frames[1:-2])

    _=sys.stdout.write("FATAL >>>\n")

    for current_frame in frames:
        lineno=current_frame.f_lineno
        filename=current_frame.f_code.co_filename

        func_name=".".join(current_frame.f_code.co_qualname.split(".")[2:])
        func_name=current_frame.f_code.co_qualname

        _=sys.stdout.write(f" {LIGHT_GRAY}{filename}:{lineno} -{RESET} {func_name}\n")

    if len(message)>0:
        _=sys.stdout.write(f" >>> {RED}{message}{RESET}\n")

    _=sys.stdout.flush()

    sys.exit(exit_code)

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

T=tp.TypeVar("T")
class Iter(tp.Generic[T]):
    "bidirectional iterator over an indexable container"
    def __init__(self,container:list[T],initial_index:int=0):
        self.container=container
        self.index=initial_index

    def copy(self)->"Iter[T]":
        "make a copy of the current state of the iterator and return the copy"
        return Iter(container=self.container,initial_index=self.index)

    @property
    def item(self)->T:
        return self.container[self.index]

    def __len__(self)->int:
        return len(self.container)

    @property
    def empty(self)->bool:
        "return True if the index of the current element exceeds the container size"
        return self.index>=len(self)

    def __getitem__(self,i:int)->T:
        "get item at self.index+i"
        return (self+i).item

    def __add__(self,o:int)->"Iter[T]":
        "advance iterator by o items"
        assert isinstance(o,int), f"{type(o)=}"
        return Iter(self.container,self.index+o)

    def __sub__(self,o:int)->"Iter[T]":
        "reverse iterator by o items"
        assert isinstance(o,int), f"{type(o)=}"
        return Iter(self.container,self.index-o)

    def offset(self,other:"Iter[T]")->int:            
        "return offset between this iterator and another (must share container)"
        if self.container is other.container:
            return self.index-other.index

        raise ValueError("cannot calculate offset between iterators not sharing containers")

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
            print(" "*padding_size,end="")

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

SPECIAL_CHARS_SET=set([special_char for special_char in "(){}[]<>,.+-/*&|%^;:=?!\"'@#"])
def is_special(c:str):
    assert len(c)==1, f"{len(c) = } ; {c = }"
    return c in SPECIAL_CHARS_SET

NUMERIC_CHARS_SET=set([n for n in "0123456789"])
def is_numeric(c:str):
    assert len(c)==1, f"{len(c) = } ; {c = }"
    return c in NUMERIC_CHARS_SET

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

    def parse_tokens(self):
        t=self

        self.tokens=[]

        def compound_symbol_present(c_sym:str,current_token:Token)->bool:
            if t.c==c_sym[0] and t.nc_rem>=(len(c_sym)-1):
                ret_s=t.c
                for i in range(1,len(c_sym)):
                    c=t.c_fut(i)
                    if c!=c_sym[i]:
                        return False
                    ret_s+=c

                # only advance if symbol has been found
                for i in range(1,len(c_sym)):
                    t.adv()

                current_token.s=ret_s
                current_token.token_type=TokenType.OPERATOR_PUNCTUATION

                return True
            return False

        def parse_terminated_literal(start_char:str,end_char:str,current_token:Token,token_type:TokenType)->bool:
            if len(current_token.s)>0:
                return False

            if t.c==start_char:
                if len(current_token.s)>0:
                    current_token.token_type=token_type
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
                        elif is_numeric(t.c):
                            match t.c:
                                case "0":
                                    current_token.s+="\0"
                                case other:
                                    fatal(f"unimplemented {other}")
                        elif t.c=="\"":
                            current_token.s+="\""
                        elif t.c=="'":
                            current_token.s+="'"
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

                current_token.token_type=token_type
                return True

            return False

        # tokenize the file (phase 3, combined with phase 2)
        current_token:Token|None=None
        while t.remaining:
            current_token=Token("",src_loc=t.current_loc(),log_loc=t.current_log_loc())

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
                if parse_terminated_literal("'","'",current_token,token_type=TokenType.LITERAL_CHAR):
                    skip_col_increment=True
                    break

                # parse a string literal
                if parse_terminated_literal('"','"',current_token,token_type=TokenType.LITERAL_STRING):
                    skip_col_increment=True
                    break

                # parse a special symbol (includes comments)
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

                    if compound_symbol_present("...",current_token):
                        break

                    # if not compound but still special:
                    current_token.token_type=TokenType.OPERATOR_PUNCTUATION
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

        return self.tokens


def main(filename:str|None=None):
    if filename is None:
        if len(sys.argv)<2:
            print("no input file")
            return

        filename=sys.argv[1]

    t=Tokenizer(filename)
    tokens=t.parse_tokens()

    # visually inspect results
    #print_tokens(tokens,mode="logical",ignore_comments=False,ignore_whitespace=True,pad_string=False)

    def filter_token(token:Token)->bool:
        " remove whitespace and comment tokens and pack into lines"
        match token.token_type:
            case TokenType.COMMENT:
                return True
            case TokenType.WHITESPACE:
                return True
            case _:
                return False

    def collapse_tokens_into_lines(tokens:list[Token])->list[list[Token]]:
        token_lines:list[list[Token]]=[[]]
        current_line_num=0
        for tok in tokens:
            if filter_token(tok):
                continue

            if tok.log_loc.line!=current_line_num:
                current_line_num=tok.log_loc.line
                token_lines.append([])

            token_lines[-1].append(tok)

        return token_lines

    token_lines=collapse_tokens_into_lines(tokens)

    # execute preprocessor (phase 4)

    class Preprocessor:
        @dataclass
        class IncludeFileReference:
            filename:str
            has_include_guard:bool=False

        @dataclass
        class Define:
            name:str
            tokens:list[Token]
            " tokens that the macro is expanded to "

            arguments:list[Token]|None=None
            has_vararg:bool=False

            generate_func:tp.Callable[[Token,list[Token]],None]|None=None
            """
            function that generates the output tokens, instead of self.tokens

            _generate_func_ pseudo-signature:
                def generate_func(src_macro_name_arg:Token,args:tp.List[Token]=self.arguments)->None
            """

        @dataclass
        class If:
            value_tokens:list[Token]
            value:bool

            do_eval:bool=True
            " indicate if this statement requires evaluation, e.g. if parent if has evaluated to zero, the syntax of the current if stack must still be evaluated but the actual value can be ignored, and will always be zero"

            first_if:bool=False
            "flag set to True only on #ifX <cond> statements"
            is_unconditional_else:bool=False
            "flag set to True only on #else directives to detect double-else or conditionals after else"

            @staticmethod
            def eval(p:"Preprocessor",tokens:list[Token],first_if:bool=False)->"Preprocessor.If":
                do_eval=True
                if len(p.if_stack)>0:
                    last_if_item=p.if_stack[-1][-1]
                    do_eval=last_if_item.do_eval and (first_if or not p.any_item_in_if_stack_evaluated_to_true())

                if not first_if:
                    if p.any_item_in_if_stack_evaluated_to_true():
                        do_eval=False

                if do_eval:
                    def remove_defchecks(tokens:list[Token])->list[Token]:
                        " execute the define operator "

                        # check if the 'defined' keyword even occurs in sequence, and return input sequence if not
                        define_found=False
                        for tok in tokens:
                            if tok.s=="defined":
                                define_found=True
                                break

                        if not define_found:
                            return tokens

                        # check for 'defined' operator and execute it
                        ret:list[Token]=[]
                        tok_index=0
                        while tok_index<len(tokens):
                            tok=tokens[tok_index]

                            if tok.s=="defined":
                                tok_index+=1
                                name_to_check_for_define=tokens[tok_index]
                                if name_to_check_for_define.s=="(":
                                    name_to_check_for_define=tokens[tok_index+1]
                                    assert tokens[tok_index+2].s==")", f"{name_to_check_for_define=} {tokens[tok_index+2]}"
                                    tok_index+=3

                                if name_to_check_for_define.s in p.defines:
                                    ret.append(Token("1",token_type=TokenType.LITERAL_NUMBER,src_loc=SourceLocation.placeholder()))
                                else:
                                    ret.append(Token("0",token_type=TokenType.LITERAL_NUMBER,src_loc=SourceLocation.placeholder()))

                                continue

                            ret.append(tok)
                            tok_index+=1

                        return ret

                    def tokens_into_str(tokens:list[Token])->str:
                        return ' '.join(tok.s for tok in tokens)

                    defcheck_removed=remove_defchecks(tokens)
                    print(f"eval define \n    {tokens_into_str(tokens)} \nto \n    {tokens_into_str(defcheck_removed)}")

                    expanded_expression=p.expand(defcheck_removed)

                    def expression_make_evalable(p:"Preprocessor",tokens:list[Token])->list[Token]:
                        ret:list[Token]=[]

                        tok_index=0
                        while tok_index<len(tokens):
                            tok=tokens[tok_index]
                            tok_index+=1

                            if tok.token_type not in (TokenType.LITERAL_CHAR, TokenType.LITERAL_NUMBER, TokenType.OPERATOR_PUNCTUATION):
                                ret.append(Token("0",token_type=TokenType.LITERAL_NUMBER,src_loc=SourceLocation.placeholder()))
                            else:
                                ret.append(tok)

                        return ret

                    evalable_expression=expression_make_evalable(p,expanded_expression)

                    print(f"evaluating expression {tokens_into_str(tokens)}, i.e. {tokens_into_str(evalable_expression)}")

                    class Expression:
                        def __init__(self):
                            pass
                        @property
                        def val(self)->int:
                            return 1
                        def print(self,indent:int=0):
                            print(f"{' '*indent}val: {self.val}")

                    @dataclass
                    class ExpressionValue(Expression):
                        value:int

                        @property
                        @tp.override
                        def val(self)->int:
                            return self.value

                        @tp.override
                        def print(self,indent:int=0):
                            print(f"{' '*indent}value: {self.val}")

                    @dataclass
                    class ExpressionLogicalNot(Expression):
                        expr:Expression

                        @property
                        @tp.override
                        def val(self)->int:
                            return int(not self.expr.val)

                        @tp.override
                        def print(self,indent:int=0):
                            print(f"{' '*indent}logicalnot: {self.val}")
                            self.expr.print(indent=indent+1)

                    @dataclass
                    class ExpressionLogicalAnd(Expression):
                        left:Expression
                        right:Expression

                        @property
                        @tp.override
                        def val(self)->int:
                            return int(bool(self.left.val) and bool(self.right.val))
                            
                        @tp.override
                        def print(self,indent:int=0):
                            print(f"{' '*indent}and: {self.val}")
                            self.left.print(indent=indent+1)
                            print(f"{' '*indent}&&")
                            self.right.print(indent=indent+1)

                    @dataclass
                    class ExpressionLogicalOr(Expression):
                        left:Expression
                        right:Expression

                        @property
                        @tp.override
                        def val(self)->int:
                            return int(bool(self.left.val) or bool(self.right.val))
                            
                        @tp.override
                        def print(self,indent:int=0):
                            print(f"{' '*indent}or: {self.val}")
                            self.left.print(indent=indent+1)
                            print(f"{' '*indent} ||")
                            self.right.print(indent=indent+1)

                    @dataclass
                    class ExpressionGreater(Expression):
                        left:Expression
                        right:Expression

                        @property
                        @tp.override
                        def val(self)->int:
                            return int(bool(self.left.val) > bool(self.right.val))
                            
                        @tp.override
                        def print(self,indent:int=0):
                            print(f"{' '*indent}greater: {self.val}")
                            self.left.print(indent=indent+1)
                            print(f"{' '*indent}>")
                            self.right.print(indent=indent+1)

                    @dataclass
                    class ExpressionGreaterOrEqual(Expression):
                        left:Expression
                        right:Expression

                        @property
                        @tp.override
                        def val(self)->int:
                            return int(bool(self.left.val) >= bool(self.right.val))
                            
                        @tp.override
                        def print(self,indent:int=0):
                            print(f"{' '*indent}greater: {self.val}")
                            self.left.print(indent=indent+1)
                            print(f"{' '*indent}>=")
                            self.right.print(indent=indent+1)

                    @dataclass
                    class ExpressionLess(Expression):
                        left:Expression
                        right:Expression

                        @property
                        @tp.override
                        def val(self)->int:
                            return int(bool(self.left.val) < bool(self.right.val))
                            
                        @tp.override
                        def print(self,indent:int=0):
                            print(f"{' '*indent}less: {self.val}")
                            self.left.print(indent=indent+1)
                            print(f"{' '*indent}<")
                            self.right.print(indent=indent+1)

                    @dataclass
                    class ExpressionLessOrEqual(Expression):
                        left:Expression
                        right:Expression

                        @property
                        @tp.override
                        def val(self)->int:
                            return int(bool(self.left.val) <= bool(self.right.val))
                            
                        @tp.override
                        def print(self,indent:int=0):
                            print(f"{' '*indent}less: {self.val}")
                            self.left.print(indent=indent+1)
                            print(f"{' '*indent}<=")
                            self.right.print(indent=indent+1)

                    class OperatorPrecedence(int,Enum):
                        GREATER=0
                        LESS=0

                        LOGIC_AND=0
                        LOGIC_OR=0

                        BINARY_AND=0,
                        BINARY_OR=0,

                        LOGIC_NOT=0,
                        BINARY_NOT=0,

                        LESS_OR_EQUAL=0,
                        GREATER_OR_EQUAL=0,

                        NONE=20

                    def parse_expression(tokens:list[Token],current_operator_precedence:OperatorPrecedence)->tuple[Expression,list[Token]]:
                        "parse expression from list of tokens. returns an expression and the leftover tokens"

                        ret:Expression=Expression()

                        while len(tokens)>0:
                            tok=tokens[0]

                            if tok.token_type==TokenType.SYMBOL:
                                fatal("")

                            elif tok.token_type==TokenType.LITERAL_NUMBER:
                                if tok.s[-1]=="L":
                                    ret=ExpressionValue(int(tok.s[:-1]))
                                else:
                                    ret=ExpressionValue(int(tok.s))

                                tokens=tokens[1:]
                                continue

                            elif tok.s=="!":
                                if current_operator_precedence<OperatorPrecedence.LOGIC_NOT:
                                    break

                                if type(ret)!=Expression:
                                    break

                                tokens=tokens[1:]
                                right,tokens=parse_expression(tokens,OperatorPrecedence.LOGIC_NOT)

                                ret=ExpressionLogicalNot(right) # type: ignore
                                continue

                            elif tok.s==">":
                                if current_operator_precedence<OperatorPrecedence.GREATER:
                                    break
                                tokens=tokens[1:]
                                right,tokens=parse_expression(tokens,OperatorPrecedence.GREATER)

                                ret=ExpressionGreater(ret,right) # type: ignore
                                continue

                            elif tok.s==">=":
                                if current_operator_precedence<OperatorPrecedence.GREATER_OR_EQUAL:
                                    break
                                tokens=tokens[1:]
                                right,tokens=parse_expression(tokens,OperatorPrecedence.GREATER_OR_EQUAL)

                                ret=ExpressionGreaterOrEqual(ret,right)
                                continue

                            elif tok.s=="<":
                                if current_operator_precedence<OperatorPrecedence.LESS:
                                    break
                                tokens=tokens[1:]
                                right,tokens=parse_expression(tokens,OperatorPrecedence.LESS)

                                ret=ExpressionLess(ret,right)
                                continue

                            elif tok.s=="<=":
                                if current_operator_precedence<OperatorPrecedence.LESS_OR_EQUAL:
                                    break
                                tokens=tokens[1:]
                                right,tokens=parse_expression(tokens,OperatorPrecedence.LESS_OR_EQUAL)

                                ret=ExpressionLessOrEqual(ret,right)
                                continue

                            elif tok.s=="&&":
                                if current_operator_precedence<OperatorPrecedence.LOGIC_AND:
                                    break
                                tokens=tokens[1:]
                                right,tokens=parse_expression(tokens,OperatorPrecedence.LOGIC_AND)

                                ret=ExpressionLogicalAnd(ret,right)
                                continue

                            elif tok.s=="||":
                                if current_operator_precedence<OperatorPrecedence.LOGIC_OR:
                                    break
                                tokens=tokens[1:]
                                right,tokens=parse_expression(tokens,OperatorPrecedence.LOGIC_OR)
                                
                                ret=ExpressionLogicalOr(ret,right)
                                continue

                            elif tok.s=="(":
                                tokens=tokens[1:]
                                ret,tokens=parse_expression(tokens,OperatorPrecedence.NONE)
                                assert tokens[0].s==")", f"expected token ), got instead {tokens[0]}"
                                tokens=tokens[1:]
                                continue

                            break

                        return ret,tokens

                    expr,leftover_tokens=parse_expression(evalable_expression,OperatorPrecedence.NONE)
                    assert len(leftover_tokens)==0, f"leftover tokens after evaluating preprocessor if statement: {tokens_into_str(leftover_tokens)}"
                    expr.print()
                    print(f"evaluted to\n{expr.val}")

                    if_value=expr.val==1
                else:
                    if_value=False
                return Preprocessor.If(tokens,if_value,first_if=first_if,do_eval=do_eval)

            @staticmethod
            def evaldef(p:"Preprocessor",tokens:list[Token],first_if:bool=False)->"Preprocessor.If":
                do_eval=True
                if len(p.if_stack)>0:
                    last_if_item=p.if_stack[-1][-1]
                    do_eval=last_if_item.do_eval and (first_if or not p.any_item_in_if_stack_evaluated_to_true())

                if not first_if:
                    if p.any_item_in_if_stack_evaluated_to_true():
                        do_eval=False

                if do_eval:
                    if_value=tokens[0].s in p.defines
                else:
                    if_value=False

                return Preprocessor.If(tokens,if_value,first_if=first_if,do_eval=do_eval)

            @staticmethod
            def evalndef(p:"Preprocessor",tokens:list[Token],first_if:bool=False)->"Preprocessor.If":
                do_eval=True
                if len(p.if_stack)>0:
                    last_if_item=p.if_stack[-1][-1]
                    do_eval=last_if_item.do_eval and (first_if or not p.any_item_in_if_stack_evaluated_to_true())

                if not first_if:
                    if p.any_item_in_if_stack_evaluated_to_true():
                        do_eval=False

                if do_eval:
                    if_value=tokens[0].s not in p.defines
                else:
                    if_value=False

                return Preprocessor.If(tokens,if_value,first_if=first_if,do_eval=do_eval)

            @staticmethod
            def evalelse(p:"Preprocessor")->"Preprocessor.If":
                do_eval=True
                if len(p.if_stack)>0:
                    last_if_item=p.if_stack[-1][-1]
                    do_eval=last_if_item.do_eval and not p.any_item_in_if_stack_evaluated_to_true()

                if do_eval:
                    if_value=True
                else:
                    if_value=False

                return Preprocessor.If([],if_value,do_eval=do_eval,is_unconditional_else=True)


        def __init__(self):
            self.lookup_dirs=[
                Path("."),
                Path("include"),
                Path("musl/include")
            ]

            self.lines:list[list[Token]]=[]
            self.current_line_index=0

            self.out_lines:list[list[Token]]=[]

            self.files_included:dict[str,Preprocessor.IncludeFileReference]={}
            self.defines:dict[str,Preprocessor.Define]={
                # from https://gcc.gnu.org/onlinedocs/cpp/Standard-Predefined-Macros.html
                "__STDC__":Preprocessor.Define("__STDC__",[Token("1",token_type=TokenType.LITERAL_NUMBER,src_loc=SourceLocation.placeholder())]),
                # __STDC_VERSION__ (from the link above): The value 199409L signifies the 1989 C standard as amended in 1994, which is
                # the current default; the value 199901L signifies the 1999 revision of the C standard; the value 201112L signifies the 
                # 2011 revision of the C standard; the value 201710L signifies the 2017 revision of the C standard (which is otherwise 
                # identical to the 2011 version apart from correction of defects). The value 202311L is used for the experimental -std=c23 
                # and -std=gnu23 modes. An unspecified value larger than 202311L is used for the experimental -std=c2y and -std=gnu2y modes.
                "__STDC_VERSION__":Preprocessor.Define("__STDC_VERSION__",[Token("202311L",token_type=TokenType.LITERAL_NUMBER,src_loc=SourceLocation.placeholder())]),

                # missing: __FILE__, __LINE__

                # not standard, but common, from https://gcc.gnu.org/onlinedocs/cpp/Common-Predefined-Macros.html:
                #   __COUNTER__, __FILE_NAME__, __ELF__, __VERSION__, __OPTIMIZE__, __OPTIMIZE_SIZE__, __NO_INLINE__, __CHAR_UNSIGNED__, __WCHAR_UNSIGNED__
                #   __CHAR_BIT__, __BYTE_ORDER__, __LP64__, __TIMESTAMP__
                #   multiple typedefs, like __INT16_TYPE__, __UINT32_TYPE__
                #   multiple size limits, like __INT_MAX__, __UINT16_MAX__
            }

            self.if_stack:list[list[Preprocessor.If]]=[]

        def add_if_stack_item(self,if_item:"Preprocessor.If"):
            if len(self.if_stack)==0 or if_item.first_if:
                self.if_stack.append([])

            self.if_stack[-1].append(if_item)

        def any_item_in_if_stack_evaluated_to_true(self)->bool:
            if len(self.if_stack)==0:
                return False

            for item in self.if_stack[-1]:
                if item.value:
                    return True

            return False

        def get_if_state(self)->bool:
            if len(self.if_stack)==0:
                return True

            last_item_in_stack=self.if_stack[-1][-1]
            return last_item_in_stack.value

        def is_empty(self)->bool:
            return self.current_line_index >= len(self.lines)

        def get_next_line(self)->list[Token]:
            ret=self.lines[self.current_line_index]
            self.current_line_index+=1
            return ret

        def add_lines(self,tokenized_lines:list[list[Token]]):
            self.lines=self.lines[:self.current_line_index]+tokenized_lines+self.lines[self.current_line_index:]


        def expand(self,tokens:list[Token]):
            "expand macros in 'tokens' argument, which are the tokens from functionally one line of code"

            in_tokens=tokens
            ret:list[Token]=[]
            while True:
                expanded_any=False
                next_tok_index=0
                while next_tok_index<len(in_tokens):
                    tok:Token=in_tokens[next_tok_index]
                    next_tok_index+=1

                    # look for target macro
                    target_macro=self.defines.get(tok.s)

                    # check for recursion (and forbid it)
                    if target_macro is not None:
                        if tok.expanded_from_macros is not None:
                            already_expanded=False
                            for already_expanded_macro in tok.expanded_from_macros:
                                if tok.s==already_expanded_macro:
                                    already_expanded=True
                                    break

                            if already_expanded:
                                target_macro=None

                    # append token directly if no macro was found
                    if target_macro is None:
                        ret.append(tok)
                        continue

                    # store flag to indicate that a macro was found
                    expanded_any=True

                    # gather macro arguments (will remain empty if macro accepts no arguments)
                    macro_arguments:dict[str,list[Token]]={}

                    # parse arguments, if macro expects any
                    define=target_macro
                    if define.arguments is not None:
                        tok=in_tokens[next_tok_index]
                        next_tok_index+=1

                        assert tok.s=="(", f"{tok} define: {define}"
                        tok=in_tokens[next_tok_index]
                        next_tok_index+=1

                        MACRO_VARARG_ARGNAME="__VA_ARGS__"
                        for define_argument in define.arguments+([MACRO_VARARG_ARGNAME] if define.has_vararg else []):
                            macro_arg:list[Token]=[]
                            nesting_depth=0
                            "count paranethesis nesting level (other delimeters are not nested, e.g. curly braces can remain unpaired)"

                            while 1:
                                if tok.s=="(":
                                    nesting_depth+=1

                                if tok.s==")":
                                    if nesting_depth==0:
                                        break

                                    nesting_depth-=1

                                if tok.s=="," and define_argument!=MACRO_VARARG_ARGNAME:
                                    tok=in_tokens[next_tok_index]
                                    next_tok_index+=1
                                    break

                                macro_arg.append(tok)

                                tok=in_tokens[next_tok_index]
                                next_tok_index+=1

                            if isinstance(define_argument,Token):
                                macro_arguments[define_argument.s]=macro_arg
                            else:
                                macro_arguments[define_argument]=macro_arg

                        # skip over trailing )
                        if tok.s!=")":
                            # may exit macro argument parsing by reaching ) or.. some other condition that is not quite clear
                            next_tok_index+=1
                            tok=in_tokens[next_tok_index]
                        assert tok.s==")", f"{tok}"

                    # expand macro body from arguments
                    out_tok_index=0
                    while out_tok_index < len(define.tokens):
                        out_tok=define.tokens[out_tok_index]
                        out_tok_index+=1

                        if out_tok.s in macro_arguments:
                            for arg in macro_arguments[out_tok.s]:
                                copied_token=arg.copy()
                                copied_token.expand_from(define.name)
                                ret.append(copied_token)

                            continue

                        # check for stringification operator
                        if out_tok.s=="#":
                            out_tok=define.tokens[out_tok_index]
                            out_tok_index+=1

                            # check for concatentation operator
                            if out_tok.s=="#":
                                last_tok=ret[-1]

                                concat_tok=define.tokens[out_tok_index]
                                if concat_tok.s in macro_arguments:
                                    assert len(macro_arguments[concat_tok.s])==1, f"can only concat individual tokens, yet got: {macro_arguments[concat_tok.s]}"
                                    last_tok.s+=macro_arguments[concat_tok.s][0].s
                                else:
                                    last_tok.s+=concat_tok.s

                                out_tok_index+=1

                                continue

                            assert out_tok.s in macro_arguments, out_tok.s

                            joined_str=""
                            last_tok:Token|None=None
                            for tok in macro_arguments[out_tok.s]:
                                if last_tok is not None and tok.token_type==TokenType.SYMBOL:
                                    joined_str+=" "
                                joined_str+=tok.s
                                last_tok=tok

                            copied_token=Token('"'+joined_str+'"',src_loc=SourceLocation.placeholder(),token_type=TokenType.LITERAL_STRING)
                            copied_token.expand_from(define.name)
                            ret.append(copied_token)

                            continue
                        
                        copied_token=out_tok.copy()
                        copied_token.expand_from(define.name)
                        ret.append(copied_token)

                # if no macro was expanded, no need to recurse -> stop expansion
                if not expanded_any:
                    break

                # if a macro was expanded, resursive expansion may be required, so newly generated tokens become new input, and new output is generated
                in_tokens=ret
                ret=[]

            return ret

        def run(self)->list[list[Token]]:
            skip_line_next=False
            line=[]
            while not self.is_empty():
                if not skip_line_next:
                    line=self.get_next_line()

                skip_line_next=False

                if len(line)==0:
                    continue

                if line[0].s=="#":
                    if len(line)<2:
                        # empty directive is allowed
                        continue

                    match line[1].s:
                        case "if":
                            if_item=Preprocessor.If.eval(self,line[2:],first_if=True)
                            self.add_if_stack_item(if_item)
                        case "ifdef":
                            # extra tokens after if value are ignored
                            if_item=Preprocessor.If.evaldef(self,line[2:],first_if=True)
                            self.add_if_stack_item(if_item)
                        case "ifndef":
                            # extra tokens after if value are ignored
                            if_item=Preprocessor.If.evalndef(self,line[2:],first_if=True)
                            self.add_if_stack_item(if_item)

                        case "elif":
                            if_item=Preprocessor.If.eval(self,line[2:])
                            self.add_if_stack_item(if_item)
                        case "elifdef":
                            # extra tokens after if value are ignored
                            if_item=Preprocessor.If.evaldef(self,line[2:])
                            self.add_if_stack_item(if_item)
                        case "elifndef":
                            # extra tokens after if value are ignored
                            if_item=Preprocessor.If.evalndef(self,line[2:])
                            self.add_if_stack_item(if_item)

                        case "else":
                            # extra tokens after if value are ignored
                            if_item=Preprocessor.If.evalelse(self)
                            self.add_if_stack_item(if_item)
                            
                        case "endif":
                            _=self.if_stack.pop()

                        case "error":
                            if not self.get_if_state():
                                continue
                        
                            error_msg=" ".join(l.s for l in line[2:])
                            print(f"{RED}ERROR - {error_msg} {RESET}")
                        case "warning":
                            if not self.get_if_state():
                                continue
                        
                            warning_msg=" ".join(l.s for l in line[2:])
                            print(f"{ORANGE}WARNING - {warning_msg} {RESET}")

                        case "pragma":
                            if not self.get_if_state():
                                continue
                        
                            match line[2].s:
                                case "once":
                                    self.files_included[line[2].src_loc.filename].has_include_guard=True
                                case other:
                                    fatal(f"unimplemented pragma {other}")

                        case "include":
                            if not self.get_if_state():
                                continue
                        
                            # extra tokens at end of inlude are legal, so we ignore them

                            file_path_str=""
                            if line[2].s[0]=="\"":
                                local_include_str=line[2].s[1:-1]

                                file_path=Path(line[2].src_loc.filename).parent/local_include_str
                                if not file_path.exists():
                                    fatal(f"unresolved: local include {local_include_str}")

                                file_path_str=str(file_path)

                            elif line[2].s=="<":
                                # collect remaining tokens in line into include string which must preserve whitespace from source
                                global_include_str=""
                                col_index=line[2].log_loc.col
                                col_index+=1
                                for tok in line[3:]:
                                    if tok.s==">":
                                        break
                                    if tok.log_loc.col!=col_index:
                                        global_include_str+=" "*(tok.log_loc.col-col_index)
                                    global_include_str+=tok.s
                                    col_index+=len(tok.s)

                                resolved_path=None
                                for lookup_dir in self.lookup_dirs:
                                    file_path=lookup_dir/global_include_str
                                    if file_path.exists():
                                        resolved_path=file_path
                                        break

                                if resolved_path is None:
                                    fatal(f"unresolved: global include {global_include_str}")

                                file_path_str=str(resolved_path)

                            else:
                                fatal(f"unexpected include directive: {line[2]}")

                            # if file has already been included and has an include guard, skip it
                            if file_path_str in self.files_included:
                                if self.files_included[file_path_str].has_include_guard:
                                    continue

                            include_file_tokenizer=Tokenizer(file_path_str)
                            include_file_tokens=include_file_tokenizer.parse_tokens()
                            include_file_lines=collapse_tokens_into_lines(include_file_tokens)

                            self.files_included[file_path_str]=Preprocessor.IncludeFileReference(file_path_str)

                            self.add_lines(include_file_lines)

                        case "define":
                            if not self.get_if_state():
                                continue

                            define=Preprocessor.Define(name=line[2].s,arguments=None,tokens=[])

                            if len(line)>3:
                                first_tok_index=3

                                if line[3].s=="(" and line[3].log_loc.col==line[2].log_loc.col+len(line[2].s):
                                    define.arguments=[]

                                    tok_index=4 # skip over opening paranthesis
                                    while line[tok_index].s!=")":
                                        argument_name=line[tok_index]
                                        if argument_name.s=="...":
                                            define.has_vararg=True
                                            tok_index+=1
                                            break # vararg must be last argument

                                        define.arguments.append(argument_name)

                                        tok_index+=1 # skip over argument name
                                        if line[tok_index].s==",":
                                            tok_index+=1 # skip over comma

                                    tok_index+=1 # skip over closing paranthesis

                                    first_tok_index=tok_index

                                for tok in line[first_tok_index:]:
                                    define.tokens.append(tok)

                            if 0:
                                define_arg_str=""
                                if define.arguments is not None:
                                    arg_str=(",".join(t.s for t in define.arguments))
                                    if define.has_vararg:
                                        arg_str+=",..."
                                    define_arg_str=" ("+arg_str+")"

                                define_body_str=""
                                if len(define.tokens)>0:
                                    define_body_str=" as "+(" ".join(t.s for t in define.tokens))

                                _=define_arg_str
                                _=define_body_str

                            self.defines[define.name]=define

                        case "undef":
                            if not self.get_if_state():
                                continue
                        
                            undef_name=line[2].s
                            if undef_name in self.defines:
                                del self.defines[undef_name]

                        case "line":
                            # https://en.cppreference.com/w/c/preprocessor/line
                            print(f"directive line unimplemented!")

                        case "embed":
                            # https://en.cppreference.com/w/c/preprocessor/embed
                            print(f"directive embed unimplemented!")

                        case unimplemented_directive:
                            print(f"directive {unimplemented_directive} unimplemented!")
                else:
                    if not self.get_if_state():
                        continue

                    skip_line_next=True

                    expand_tokens:list[Token]=[]
                    while line[0].s!="#":
                        expand_tokens.extend(line)

                        if self.is_empty():
                            break

                        line=self.get_next_line()

                    new_line=self.expand(expand_tokens)

                    self.out_lines.append(new_line)

            return self.out_lines

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

    print("tokens going into phase 7:")
    for tok in tokens:
        print(f"{tok.s}",end=" ")
    print("")

    class CType:
        "basic c type class"
        def __init__(self):
            self.is_static:bool=False
            self.is_extern:bool=False

            self.is_atomic:bool=False

            self.is_const:bool=False

            self.is_signed:bool|None=None

            self.is_explicit_struct:bool=False
            self.is_explicit_enum:bool=False
            self.is_explicit_union:bool=False

            self.base_type:CType|None=None

        def is_empty_default(self)->bool:
            "returns True if the type has every field set to its default"
            return self.is_static==False and self.is_extern==False and self.is_atomic==False and self.is_const==False and self.is_signed==None and self.base_type==None

        @tp.override
        def __str__(self)->str:
            ret=""
            if self.is_static:
                ret+="static "
            if self.is_extern:
                ret+="extern "
            if self.is_atomic:
                ret+="atomic "
            if self.is_const:
                ret+="const "
            if self.is_signed is not None:
                if self.is_signed:
                    ret+="signed "
                else:
                    ret+="unsigned "

            if self.is_explicit_struct:
                ret+="struct "
            if self.is_explicit_enum:
                ret+="enum "
            if self.is_explicit_union:
                ret+="union "

            return ret

        def print(self,indent:int):
            name_str=str(self)
            if len(name_str)>0:
                name_str+=" "

            print(" "*indent+name_str,end="")
            if self.base_type is not None:
                print("base:")
                self.base_type.print(indent+1)
            else:
                print("")

    class CTypePrimitive(CType):
        def __init__(self,name:str):
            super().__init__()
            self.name=name

        @tp.override
        def print(self,indent:int):
            print(" "*indent+self.name)

    class CTypePointer(CType):
        "pointer to some other type (uses CType.base_type as base)"
        def __init__(self,base_type:CType):
            super().__init__()
            self.base_type=base_type

        @tp.override
        def print(self,indent:int):
            assert self.base_type is not None
            print(" "*indent+"ptr to:")
            self.base_type.print(indent+1)

    class CTypeFunction(CType):
        "function type"
        def __init__(self,return_type:CType,arguments:list["Symbol"]):
            super().__init__()
            self.return_type=return_type
            self.arguments=arguments

        @tp.override
        def print(self,indent:int):
            print(" "*indent+f"fn:")
            print(" "*(indent+1)+"args:")
            for arg in self.arguments:
                if arg.name is None:
                    print(" "*(indent+2)+f"<anon>:")
                    arg.ctype.print(indent+3)
                else:
                    print(" "*(indent+2)+f"{arg.name.s}:")
                    arg.ctype.print(indent+3)

            print(" "*(indent+1)+"ret:")
            self.return_type.print(indent+2)


    class Symbol:
        def __init__(self,ctype:CType,name:Token|None=None):
            self.name=name
            self.ctype=ctype

        @tp.override
        def __str__(self)->str:
            if self.name is not None:
                return f"{self.name.s}: {self.ctype}"
            else:
                return f"<anon>: {self.ctype}"

    class AstValue:
        def __init__(self):
            pass

        def print(self,indent:int):
            pass

    class AstValueNumericLiteral(AstValue):
        def __init__(self,value:str|int|float,ctype:CType):
            super().__init__()
            self.value=value
            self.ctype:CType=ctype

        @tp.override
        def print(self,indent:int):
            print(" "*indent+"number:")
            print(" "*(indent+1)+"literal: "+str(self.value))
            print(" "*(indent+1)+"type: ")
            self.ctype.print(indent+2)

    class AstValueSymbolref(AstValue):
        def __init__(self,symbol_to_ref:Symbol):
            super().__init__()
            self.symbol=symbol_to_ref

        @tp.override
        def print(self,indent:int):
            assert self.symbol.name is not None, "should be unreachable"
            print(" "*indent+"ref sym: "+self.symbol.name.s)

    class AstFunctionCall(AstValue):
        def __init__(self,func:AstValue,arguments:list[AstValue]):
            super().__init__()
            self.func=func
            self.arguments=arguments

        @tp.override
        def print(self,indent:int):
            print(" "*indent+"call:")
            self.func.print(indent+1)

            print(" "*(indent+1)+"args:")
            for arg in self.arguments:
                arg.print(indent+2)

    class AstOperationKind(str,Enum):
        GREATER_THAN=">"
        LESS_THAN="<"
        PLUS="+"
        MINUS="-"
        INCREMENT="++"
        DECREMENT="--"

        SUBSCRIPT="subscript"

    class AstOperation(AstValue):
        def __init__(self,operation:str|AstOperationKind,val0:AstValue,val1:AstValue|None=None,val2:AstValue|None=None):
            super().__init__()
            self.operation=AstOperationKind(operation)
            self.val0=val0
            self.val1=val1
            self.val2=val2

        @tp.override
        def print(self,indent:int):
            match self.operation:
                case AstOperationKind.GREATER_THAN:
                    print(" "*indent+"op:")
                    self.val0.print(indent+1)
                    print(" "*(indent+1)+">")
                    assert self.val1 is not None
                    self.val1.print(indent+1)
                case _:
                    fatal("unimplemented")

    class Statement:
        "base class for any statement"

        def print(self,indent:int):
            pass

    class StatementReturn(Statement):
        def __init__(self,value:AstValue|None):
            super().__init__()
            self.value=value

        @tp.override
        def print(self,indent:int):
            p=" "*indent+"return"
            if self.value is not None:
                p+=":"

            print(p)
            if self.value is not None:
                self.value.print(indent+1)

    class StatementValue(Statement):
        def __init__(self,value:AstValue):
            super().__init__()
            self.value=value

        @tp.override
        def print(self,indent:int):
            print(" "*indent+"value:")
            self.value.print(indent+1)

    class SymbolDef(Statement):
        "statement symbol definition"
        def __init__(self,symbol:Symbol):
            super().__init__()
            self.symbol=symbol

        @tp.override
        def print(self,indent:int):
            if self.symbol.name is None:
                print(" "*indent+f"def <anon>:")
            else:
                print(" "*indent+f"def {self.symbol.name.s}:")

            self.symbol.ctype.print(indent+1)

    class Block:
        "block/scope containing a list of statements (which may include other Blocks) and derived information, like new types"
        def __init__(self,
            types:dict[str,CType]|None=None,

            struct_types:dict[str,CType]|None=None,
            enum_types:dict[str,CType]|None=None,
            union_types:dict[str,CType]|None=None,

            symbols:dict[str,Symbol]|None=None,

            parent:"Block|None"=None,
        ):
            self.types:dict[str,CType]=types.copy() if types is not None else {}

            self.struct_types:dict[str,CType]=struct_types.copy() if struct_types is not None else {}
            self.enum_types:dict[str,CType]=enum_types.copy() if enum_types is not None else {}
            self.union_types:dict[str,CType]=union_types.copy() if union_types is not None else {}

            self.symbols:dict[str,Symbol]=symbols.copy() if symbols is not None else {}
            self.statements:list[Statement]=[]

            self.parent=parent

        def print(self,indent:int):
            for statement in self.statements:
                statement.print(indent)

        def getTypeByName(self,name:str)->CType|None:
            if name in self.types:
                return self.types[name]

            if self.parent is not None:
                return self.parent.getTypeByName(name)

            return None

        def getSymbolByName(self,name:str)->Symbol|None:
            if name in self.symbols:
                return self.symbols[name]

            if self.parent is not None:
                return self.parent.getSymbolByName(name)

            return None

        def addStatement(self,statement:Statement,ingest_symbols:bool=True):
            if ingest_symbols and isinstance(statement,SymbolDef):
                self.addSymbol(statement.symbol)

            self.statements.append(statement)

        def addSymbol(self,symbol:Symbol):
            if symbol.name is not None:
                self.symbols[symbol.name.s]=symbol

    class AstForLoop(Block,Statement):
        def __init__(self,
            init_statement:Statement,
            condition:AstValue|None=None,
            step_statement:AstValue|None=None,

            parent:Block|None=None,
        ):
            super().__init__(parent=parent)
            self.init=init_statement
            self.condition=condition
            self.step=step_statement

            self.addStatement(self.init)

        @tp.override
        def print(self,indent:int):
            print(" "*indent+"for:")
            print(" "*(indent+1)+"init:")
            self.init.print(indent+2)
            print(" "*(indent+1)+"cond:")
            if self.condition is not None:
                self.condition.print(indent+2)
            print(" "*(indent+1)+"step:")
            if self.step is not None:
                self.step.print(indent+2)

    class AstBlock(Block,Statement):
        def __init__(self,
            parent:Block|None=None,
        ):
            super().__init__(parent=parent)

        @tp.override
        def print(self,indent:int):
            print(" "*indent+"block:")
            Block.print(self,indent+1)

    class AstFunction(Block,Statement):
        def __init__(self,func_type:CTypeFunction,**kwargs:tp.Any|None):
            super().__init__(**kwargs)
            self.func_type=func_type

            for arg in self.func_type.arguments:
                self.addSymbol(arg)

        @tp.override
        def print(self,indent:int):
            self.func_type.print(indent)
            for statement in self.statements:
                statement.print(indent+1)

    class Ast:
        "represents one "
        def __init__(self,tokens:list[Token]):
            self.t=Iter(tokens)

            self.block:Block=Block(types={
                n:CTypePrimitive(n)
                for n
                in ["void","char","int","bool"]
            })

            self.parse_block()

            if not self.t.empty:
                fatal(f"leftover tokens at end of file: {str(self.t[0])} ...")

        def parse_block(self):
            "parse tokens as a series of statements into self.block, stop when something that is not valid syntax is encountered"
            while not self.t.empty:
                statement=self.parse_statement()
                if statement is None:
                    break

                if not isinstance(statement,AstFunction):
                    self.block.addStatement(statement,ingest_symbols=False)
                else:
                    self.block.addStatement(statement)

        def parse_statement(self)->Statement|None:
            """
            parse a terminated statement
            """

            old_t=self.t.copy()

            match self.tok.s:
                case "{":
                    self.t+=1

                    old_block=self.block
                    
                    astblock=AstBlock(parent=self.block)
                    self.block=astblock
                    self.parse_block()

                    assert self.t[0].s=="}", f"got instead {self.t[0].s}"
                    self.t+=1

                    self.block=old_block

                    return astblock

                case "typedef":
                    fatal("typedef unimplemented")

                case "switch":
                    fatal("switch unimplemented")
                case "while":
                    fatal("while unimplemented")
                case "for":
                    self.t+=1

                    assert self.t[0].s=="("
                    self.t+=1

                    # parse for init
                    init_statement=self.parse_statement()
                    assert init_statement is not None

                    for_block=AstForLoop(init_statement,parent=self.block)

                    old_block=self.block
                    self.block=for_block

                    # parse for condition
                    condition=self.parse_value()
                    for_block.condition=condition

                    assert self.t[0].s==";", f"got instead {self.t[0].s}"
                    self.t+=1

                    # parse for step
                    step_statement=self.parse_value()
                    for_block.step=step_statement

                    assert self.t[0].s==")", f"got instead {self.t[0].s}"
                    self.t+=1

                    for_body_statement=self.parse_statement()
                    assert for_body_statement is not None
                    for_body_statement.print(0)

                    self.block=old_block

                    return for_body_statement
                    
                case "break":
                    fatal("break unimplemented")
                case "continue":
                    fatal("continue unimplemented")

                case "return":
                    self.t+=1

                    value=self.parse_value()

                    assert self.t[0].s==";", f"expected ; got instead {self.t[0].s}"
                    self.t+=1

                    return StatementReturn(value)

                case "goto":
                    fatal("goto unimplemented")

                case _:
                    symbol_def=self.parse_symbol_definition()
                    if symbol_def is not None:
                        self.block.addSymbol(symbol_def.symbol)

                        if isinstance(symbol_def.symbol.ctype,CTypeFunction):
                            func_def=self.parse_function_definition(func_type=symbol_def.symbol.ctype)
                            if func_def is None:
                                print("failed to parse function definition for decl:")
                                symbol_def.symbol.ctype.print(0)
                                fatal(f"at {symbol_def.symbol.name.s if symbol_def.symbol.name is not None else '<anon>'}")

                            return func_def

                        assert self.t[0].s==";"
                        self.t+=1

                        return symbol_def

                    value=self.parse_value()
                    if value is not None:
                        assert self.t[0].s==";"
                        self.t+=1

                        return StatementValue(value)

                    symdef=self.parse_symbol_definition()
                    if symdef is not None:
                        assert self.t[0].s==";"
                        self.t+=1

                        return symdef

            self.t=old_t
            return None

        def parse_value(self)->AstValue|None:
            old_t=self.t.copy()

            ret=None
            while 1:
                match self.t[0].token_type:
                    case TokenType.LITERAL_CHAR:
                        if ret is not None: break

                        chartype=self.block.getTypeByName("char")
                        assert chartype is not None
                        ret=AstValueNumericLiteral(self.t[0].s,chartype)
                        self.t+=1

                    case TokenType.LITERAL_NUMBER:
                        if ret is not None: break
                        
                        # TODO actually decide the type of the numeric
                        numtype=self.block.getTypeByName("int")
                        assert numtype is not None
                        ret=AstValueNumericLiteral(self.t[0].s,numtype)
                        self.t+=1

                    case TokenType.LITERAL_STRING:
                        if ret is not None: break
                        
                        # TODO actually decide the type of the numeric
                        cstring_type=self.block.getTypeByName("char")
                        assert cstring_type is not None
                        cstring_type=CTypePointer(cstring_type)
                        ret=AstValueNumericLiteral(self.t[0].s,cstring_type)
                        self.t+=1

                    case TokenType.SYMBOL:
                        if ret is not None: break

                        symbol=self.block.getSymbolByName(self.t[0].s)
                        if symbol is None:
                            break

                        ret=AstValueSymbolref(symbol)
                        self.t+=1

                    case TokenType.OPERATOR_PUNCTUATION:
                        match self.t[0].s:
                            case "+":
                                op_str=self.t[0].s

                                self.t+=1

                                if ret is None:
                                    ret=self.parse_value()
                                    if ret is None: break
                                    ret=AstOperation(op_str,ret)
                                    continue

                                rhv=self.parse_value()
                                if rhv is None: break
                                ret=AstOperation(op_str,ret,rhv)

                            case "-":
                                op_str=self.t[0].s

                                self.t+=1

                                if ret is None:
                                    ret=self.parse_value()
                                    if ret is None: break
                                    ret=AstOperation(op_str,ret)
                                    continue

                                rhv=self.parse_value()
                                if rhv is None: break
                                ret=AstOperation(op_str,ret,rhv)

                            case "<":
                                if ret is None: break

                                self.t+=1

                                rhv=self.parse_value()
                                if rhv is None: break
                                ret=AstOperation("<",ret,rhv)

                            case "++":
                                self.t+=1

                                if ret is None:
                                    ret=self.parse_value()

                                if ret is None: break

                                ret=AstOperation("++",ret)

                            case "(":
                                if ret is None:
                                    fatal("unimplemented ( precedence override")

                                # TODO check that ret is callable

                                self.t+=1

                                argument_values:list[AstValue]=[]
                                while arg_value:=self.parse_value():
                                    argument_values.append(arg_value)

                                    if self.t[0].s==",":
                                        self.t+=1
                                        continue

                                    break

                                assert self.t[0].s==")"
                                self.t+=1

                                ret=AstFunctionCall(ret,argument_values)

                            case ";":
                                break

                            case _:
                                break

                    case other:
                        fatal(f"unimplemented : {other}")

            if ret is None:
                self.t=old_t

            return ret

        def parse_function_definition(self,func_type:CTypeFunction)->AstFunction|None:
            if self.t[0].s!="{":
                return None

            self.t+=1

            outer_block=self.block

            func_block=AstFunction(func_type,parent=outer_block)
            self.block=func_block
            self.parse_block()

            self.block=outer_block

            assert self.t[0].s=="}", f"got instead {self.t[0].s}"

            self.t+=1

            return func_block

        def parse_symbol_definition(self)->SymbolDef|None:
            old_t=self.t.copy()

            ctype:CType=CType()
            symbol:Symbol|None=None

            while not self.t.empty:
                match self.tok.s:
                    case "extern":
                        ctype.is_extern=True
                        self.t+=1
                        continue

                    case "static":
                        ctype.is_static=True
                        self.t+=1
                        continue

                    case "signed":
                        ctype.is_signed=True
                        self.t+=1
                        continue

                    case "unsigned":
                        ctype.is_signed=False
                        self.t+=1
                        continue

                    case "*":
                        ctype=CTypePointer(ctype)
                        self.t+=1
                        continue

                    case "struct":
                        ctype.is_explicit_struct=True
                        self.t+=1
                        continue

                    case "enum":
                        ctype.is_explicit_enum=True
                        self.t+=1
                        continue

                    case "union":
                        ctype.is_explicit_union=True
                        self.t+=1
                        continue

                    case other:
                        if other =="(":
                            if symbol is not None:
                                arguments:list[Symbol]=[]

                                self.t+=1
                                while self.tok.s!=")":
                                    symbol_def=self.parse_symbol_definition()
                                    if symbol_def is None:
                                        break

                                    arguments.append(symbol_def.symbol)
                                    if self.tok.s==",":
                                        self.t+=1
                                        continue

                                    break

                                assert self.tok.s==")"

                                ctype=CTypeFunction(ctype,arguments)
                                symbol.ctype=ctype

                                self.t+=1
                                continue

                            fatal(f"unimplemented - {other}")

                        other_as_type=self.block.getTypeByName(other)
                        if other_as_type is not None:
                            ctype.base_type=other_as_type
                            self.t+=1
                            continue

                        elif ctype.is_explicit_struct and other in self.block.struct_types:
                            ctype.base_type=self.block.struct_types[other]
                            self.t+=1
                            continue

                        elif ctype.is_explicit_enum and other in self.block.enum_types:
                            ctype.base_type=self.block.enum_types[other]
                            self.t+=1
                            continue

                        elif ctype.is_explicit_union and other in self.block.union_types:
                            ctype.base_type=self.block.union_types[other]
                            self.t+=1
                            continue

                        elif other=="=":
                            self.t+=1
                            value=self.parse_value()
                            assert value is not None, "no value for symbol init"
                            continue

                        else:
                            if not self.tok.is_valid_symbol():
                                break

                            if ctype.is_empty_default():
                                break

                            symbol=Symbol(ctype=ctype,name=self.tok)
                            self.t+=1
                            continue 

                break

            if symbol is None:
                self.t=old_t
                return None

            return SymbolDef(symbol)

        @property
        def tok(self)->Token:
            return self.t.item

    ast=Ast(tokens)

    print(f"{GREEN}ast:{RESET}")
    ast.block.print(0)

    # phase 8 - linking

    return

if __name__=="__main__":
    start_time=time.perf_counter()

    test_files=Path("test").glob("test*.c")
    test_files=sorted(test_files)

    for f_path in test_files[:10]:
        f=str(f_path)

        print(f"{ORANGE}{f}{RESET}")
        main(f)

    end_time=time.perf_counter()

    print(f"ran in {(end_time-start_time):.4f}s")