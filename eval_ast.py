import sys, os, io, time
from dataclasses import dataclass, field
import typing as tp
from enum import Enum
from pathlib import Path
from glob import glob

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

    GLOBAL_HEADER=9 # local header is just a string

    LITERAL_STRING=0x10
    LITERAL_CHAR=0x11
    LITERAL_NUMBER=0x12

@dataclass
class Token:
    s:str
    src_loc:SourceLocation
    token_type:TokenType=TokenType.SYMBOL
    log_loc:SourceLocation=field(default_factory=lambda:SourceLocation.invalid())

    expanded_from_macros:tp.Optional[tp.List[str]]=None

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

SPECIAL_CHARS_SET=set([special_char for special_char in "(){}[]<>,.+-/*&|%^;:=?!\"'@#"])
def is_special(c):
    return c in SPECIAL_CHARS_SET

NUMERIC_CHARS_SET=set([n for n in "0123456789"])
def is_numeric(c):
    return c in NUMERIC_CHARS_SET

class Tokenizer:
    " utility class to convert file characters into tokens "

    def __init__(self,filename:str):
        self.filename=filename

        file=io.open(filename)
        self.file_contents=file.read()
        file.close()
        self.file_index=0

        self.logical_line_index:int=0
        self.logical_col_index:int=0
        self.line_index:int=0
        self.col_index:int=0

        self.tokens:tp.List[Token]=[]

    @property
    def c(self):
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

    def c_fut(self,n):
        " return character in current line n positions in advance of current pointer "
        return self.file_contents[self.file_index+n]

    def current_loc(self):
        " return source location of current pointer into file "

        return SourceLocation(self.filename,self.line_index,self.col_index)

    def current_log_loc(self):
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
                        elif is_numeric(t.c):
                            match t.c:
                                case "0":
                                    current_token.s+="\0"
                                case other:
                                    fatal(f"unimplemented {other}")
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
                if parse_terminated_literal("'","'",current_token):
                    skip_col_increment=True
                    break

                # parse a string literal
                if parse_terminated_literal('"','"',current_token):
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


def main():
    if len(sys.argv)<2:
        print("no input file")
        return

    filename=sys.argv[1]

    start_time=time.perf_counter()
    def measure_timestep(msg:str):
        nonlocal start_time
        current_time=time.perf_counter()
        time_elapsed_s=current_time-start_time
        print(f"{msg} elapsed {time_elapsed_s:.3f}s")
        start_time=current_time

    t=Tokenizer(filename)
    tokens=t.parse_tokens()

    # visually inspect results
    #print_tokens(tokens,mode="logical",ignore_comments=False,ignore_whitespace=True,pad_string=False)

    # remove whitespace and comment tokens and pack into lines
    def filter_token(token:Token)->bool:
        match token.token_type:
            case TokenType.COMMENT:
                return True
            case TokenType.WHITESPACE:
                return True
            case tok:
                return False

    def collapse_tokens_into_lines(tokens)->tp.List[tp.List[Token]]:
        token_lines=[[]]
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
            tokens:tp.List[Token]
            " tokens that the macro is expanded to "

            arguments:tp.Optional[tp.List[Token]]=None
            has_vararg:bool=False

            generate_func:tp.Optional[tp.Callable[[Token,tp.List[Token]],None]]=None
            """
            function that generates the output tokens, instead of self.tokens

            _generate_func_ pseudo-signature:
                def generate_func(src_macro_name_arg:Token,args:tp.List[Token]=self.arguments)->None
            """

        @dataclass
        class If:
            value_tokens:tp.List[Token]
            value:bool

            do_eval:bool=True
            " indicate if this statement requires evaluation, e.g. if parent if has evaluated to zero, the syntax of the current if stack must still be evaluated but the actual value can be ignored, and will always be zero"

            first_if:bool=False
            "flag set to True only on #ifX <cond> statements"
            is_unconditional_else:bool=False
            "flag set to True only on #else directives to detect double-else or conditionals after else"

            @staticmethod
            def eval(p:"Preprocessor",tokens:tp.List[Token],first_if:bool=False)->"Preprocessor.If":
                do_eval=True
                if len(p.if_stack)>0:
                    last_if_item=p.if_stack[-1][-1]
                    do_eval=last_if_item.do_eval and (first_if or not p.any_item_in_if_stack_evaluated_to_true())

                if not first_if:
                    if p.any_item_in_if_stack_evaluated_to_true():
                        do_eval=False

                if do_eval:
                    def remove_defchecks(tokens:tp.List[Token])->tp.List[Token]:
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
                        ret=[]
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

                    def tokens_into_str(tokens:tp.List[Token])->str:
                        return ' '.join(tok.s for tok in tokens)

                    defcheck_removed=remove_defchecks(tokens)
                    print(f"eval define \n    {tokens_into_str(tokens)} \nto \n    {tokens_into_str(defcheck_removed)}")

                    expanded_expression=p.expand(defcheck_removed)

                    def expression_make_evalable(p:"Preprocessor",tokens:tp.List[Token])->tp.List[Token]:
                        ret=[]

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

                    # TODO actually evaluate the expression
                    class Expression:
                        def __init__(self):
                            pass
                        @property
                        def val(self)->int:
                            return 1

                    @dataclass
                    class ExpressionValue(Expression):
                        value:int

                        @property
                        def val(self)->int:
                            return self.value

                    @dataclass
                    class ExpressionAnd:
                        left:tp.Type[Expression]
                        right:tp.Type[Expression]

                        @property
                        def val(self)->int:
                            return self.left.val and self.right.val

                    @dataclass
                    class ExpressionOr:
                        left:tp.Type[Expression]
                        right:tp.Type[Expression]

                        @property
                        def val(self)->int:
                            return self.left.val or self.right.val

                    @dataclass
                    class ExpressionGreater:
                        left:tp.Type[Expression]
                        right:tp.Type[Expression]

                        @property
                        def val(self)->int:
                            return self.left.val > self.right.val

                    @dataclass
                    class ExpressionLess:
                        left:tp.Type[Expression]
                        right:tp.Type[Expression]

                        @property
                        def val(self):
                            return self.left.val < self.right.val

                    class OperatorPrecedence(int,Enum):
                        GREATER=0
                        LESS=0
                        AND=0
                        OR=0

                        NONE=20

                    def parse_expression(tokens:tp.List[Token],current_operator_precedence:OperatorPrecedence)->tp.Tuple[Expression,tp.List[Token]]:
                        "parse expression from list of tokens. returns an expression and the leftover tokens"

                        ret=Expression()

                        while len(tokens)>0:
                            tok=tokens[0]

                            if tok.token_type==TokenType.SYMBOL:
                                fatal("")
                            elif tok.token_type==TokenType.LITERAL_NUMBER:
                                ret=ExpressionValue(int(tok.s))

                                tokens=tokens[1:]
                                continue
                            elif tok.s==">":
                                if current_operator_precedence<OperatorPrecedence.GREATER:
                                    break
                                tokens=tokens[1:]
                                right,tokens=parse_expression(tokens,OperatorPrecedence.GREATER)

                                ret=ExpressionGreater(ret,right)
                                continue
                            elif tok.s=="<":
                                if current_operator_precedence<OperatorPrecedence.LESS:
                                    break
                                tokens=tokens[1:]
                                right,tokens=parse_expression(tokens,OperatorPrecedence.LESS)

                                ret=ExpressionLess(ret,right)
                                continue
                            elif tok.s=="&&":
                                if current_operator_precedence<OperatorPrecedence.AND:
                                    break
                                tokens=tokens[1:]
                                right,tokens=parse_expression(tokens,OperatorPrecedence.AND)

                                ret=ExpressionAnd(ret,right)
                                continue
                            elif tok.s=="||":
                                if current_operator_precedence<OperatorPrecedence.OR:
                                    break
                                tokens=tokens[1:]
                                right,tokens=parse_expression(tokens,OperatorPrecedence.OR)
                                
                                ret=ExpressionOr(ret,right)
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
                    print(f"evaluted to\n{expr.val}")

                    if_value=expr.val==1
                else:
                    if_value=False
                return Preprocessor.If(tokens,if_value,first_if=first_if,do_eval=do_eval)

            @staticmethod
            def evaldef(p:"Preprocessor",tokens:tp.List[Token],first_if:bool=False)->"Preprocessor.If":
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
            def evalndef(p:"Preprocessor",tokens:tp.List[Token],first_if:bool=False)->"Preprocessor.If":
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

            self.lines:tp.List[tp.List[Token]]=[]
            self.current_line_index=0

            self.out_lines:tp.List[tp.List[Token]]=[]

            self.files_included:tp.Dict[str,Preprocessor.IncludeFileReference]={}
            self.defines:tp.Dict[str,Preprocessor.Define]={
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

            self.if_stack:tp.List[tp.List[Preprocessor.If]]=[]

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

        def get_next_line(self)->tp.List[Token]:
            ret=self.lines[self.current_line_index]
            self.current_line_index+=1
            return ret

        def add_lines(self,tokenized_lines:tp.List[tp.List[Token]]):
            self.lines=self.lines[:self.current_line_index]+tokenized_lines+self.lines[self.current_line_index:]


        def expand(self,tokens:tp.List[Token]):
            "expand macros in 'tokens' argument, which are the tokens from functionally one line of code"

            in_tokens=tokens
            ret:tp.List[Token]=[]
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
                    macro_arguments:tp.Dict[str,tp.List[Token]]={}

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
                            macro_arg:tp.List[Token]=[]
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
                            last_tok:tp.Optional[Token]=None
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

        def run(self)->tp.List[tp.List[Token]]:
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
                            self.if_stack.pop()

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

                            define_arg_str=""
                            if define.arguments is not None:
                                arg_str=(",".join(t.s for t in define.arguments))
                                if define.has_vararg:
                                    arg_str+=",..."
                                define_arg_str=" ("+arg_str+")"

                            define_body_str=""
                            if len(define.tokens)>0:
                                define_body_str=" as "+(" ".join(t.s for t in define.tokens))

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

                    expand_tokens=[]
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

    #return
    for line in preprocessed_lines:
        for tok in line:
            print(f"{tok.s} ",end="")
        print("")

    return


if __name__=="__main__":
    start_time=time.perf_counter()

    main()

    end_time=time.perf_counter()

    print(f"ran in {(end_time-start_time):.4f}s")