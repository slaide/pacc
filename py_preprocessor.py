import sys, io, time
from dataclasses import dataclass, field
import typing as tp
from enum import Enum
from pathlib import Path
import inspect
import re

from py_util import *

from py_tokenizer import *

VARARG_ARGNAME:str="..."

def test_filter_token(token:Token)->bool:
    " remove whitespace and comment tokens and pack into lines"
    match token.token_type:
        case TokenType.COMMENT:
            return True
        case TokenType.WHITESPACE:
            return True
        case _:
            return False

def sort_tokens_into_lines(tokens:list[Token])->list[list[Token]]:
    token_lines:list[list[Token]]=[[]]
    current_line_num=0
    for tok in tokens:
        if test_filter_token(tok):
            continue

        if tok.log_loc.line!=current_line_num:
            current_line_num=tok.log_loc.line
            token_lines.append([])

        token_lines[-1].append(tok)

    return token_lines

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
        return int(self.left.val > self.right.val)
        
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
        return int(self.left.val >= self.right.val)
        
    @tp.override
    def print(self,indent:int=0):
        print(f"{' '*indent}greater or equal: {self.val}")
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
        return int(self.left.val < self.right.val)
        
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
        return int(self.left.val <= self.right.val)
        
    @tp.override
    def print(self,indent:int=0):
        print(f"{' '*indent}less: {self.val}")
        self.left.print(indent=indent+1)
        print(f"{' '*indent}<=")
        self.right.print(indent=indent+1)

@dataclass
class ExpressionEqual(Expression):
    left:Expression
    right:Expression

    @property
    @tp.override
    def val(self)->int:
        return int(self.left.val == self.right.val)
        
    @tp.override
    def print(self,indent:int=0):
        print(f"{' '*indent}equal: {self.val}")
        self.left.print(indent=indent+1)
        print(f"{' '*indent}==")
        self.right.print(indent=indent+1)

@dataclass
class ExpressionUnequal(Expression):
    left:Expression
    right:Expression

    @property
    @tp.override
    def val(self)->int:
        return int(self.left.val != self.right.val)
        
    @tp.override
    def print(self,indent:int=0):
        print(f"{' '*indent}unequal: {self.val}")
        self.left.print(indent=indent+1)
        print(f"{' '*indent}!=")
        self.right.print(indent=indent+1)

@dataclass
class ExpressionAddition(Expression):
    left:Expression
    right:Expression

    @property
    @tp.override
    def val(self)->int:
        return int(self.left.val + self.right.val)
        
    @tp.override
    def print(self,indent:int=0):
        print(f"{' '*indent}addition: {self.val}")
        self.left.print(indent=indent+1)
        print(f"{' '*indent}+")
        self.right.print(indent=indent+1)

@dataclass
class ExpressionSubtraction(Expression):
    left:Expression
    right:Expression

    @property
    @tp.override
    def val(self)->int:
        return int(self.left.val - self.right.val)
        
    @tp.override
    def print(self,indent:int=0):
        print(f"{' '*indent}subtraction: {self.val}")
        self.left.print(indent=indent+1)
        print(f"{' '*indent}-")
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

    ADDITION=0,
    SUBTRACTION=0,

    EQUAL=0,
    UNEQUAL=0,

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

        elif tok.s=="==":
            if current_operator_precedence<OperatorPrecedence.EQUAL:
                break
            tokens=tokens[1:]
            right,tokens=parse_expression(tokens,OperatorPrecedence.EQUAL)

            ret=ExpressionEqual(ret,right)
            continue

        elif tok.s=="!=":
            if current_operator_precedence<OperatorPrecedence.UNEQUAL:
                break
            tokens=tokens[1:]
            right,tokens=parse_expression(tokens,OperatorPrecedence.UNEQUAL)

            ret=ExpressionUnequal(ret,right)
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

        elif tok.s=="+":
            if current_operator_precedence<OperatorPrecedence.ADDITION:
                break
            tokens=tokens[1:]
            right,tokens=parse_expression(tokens,OperatorPrecedence.ADDITION)
            
            ret=ExpressionAddition(ret,right)
            continue

        elif tok.s=="(":
            tokens=tokens[1:]
            ret,tokens=parse_expression(tokens,OperatorPrecedence.NONE)
            assert tokens[0].s==")", f"expected token ), got instead {tokens[0]}"
            tokens=tokens[1:]
            continue

        break

    return ret,tokens

def tokens_into_str(tokens:list[Token])->str:
    "join all tokens into a string (mostly for debugging)"
    return ' '.join(tok.s for tok in tokens)

def expression_make_evalable(p:"Preprocessor",tokens:list[Token])->list[Token]:
    "resolve unknown tokens to ensure this list of tokens can be parsed as a series of purely numeric preprocessor operations"
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

def remove_defchecks(p:"Preprocessor",tokens:list[Token])->list[Token]:
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


    time_spent_expanding0=0.0
    time_spent_expanding1=0.0

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
        def eval(p:"Preprocessor",tokens:list[Token],first_if:bool=False,_debug_print:bool=False)->"Preprocessor.If":
            do_eval=True
            if len(p.if_stack)>0:
                last_if_item=p.if_stack[-1][-1]
                do_eval=last_if_item.do_eval and (first_if or not p.any_item_in_if_stack_evaluated_to_true())

            if not first_if:
                if p.any_item_in_if_stack_evaluated_to_true():
                    do_eval=False

            if do_eval:
                defcheck_removed=remove_defchecks(p,tokens)
                if _debug_print:
                    print(f"eval define \n    {tokens_into_str(tokens)} \nto \n    {tokens_into_str(defcheck_removed)}")

                expanded_expression=p.expand(defcheck_removed)


                evalable_expression=expression_make_evalable(p,expanded_expression)

                if _debug_print:
                    print(f"evaluating expression {tokens_into_str(tokens)}, i.e. {tokens_into_str(evalable_expression)}")

                expr,leftover_tokens=parse_expression(evalable_expression,OperatorPrecedence.NONE)

                assert len(leftover_tokens)==0, f"leftover tokens after evaluating preprocessor if statement: {tokens_into_str(leftover_tokens)}"
                
                if _debug_print:
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
        "check if a new line should be tokenized or skipped (returns True if the line should be processed)"
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
                target_macro=self.defines.get(tok.s) if tok.token_type==TokenType.SYMBOL else None

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
        skip_fetch_line_next=False
        line=[]
        while not self.is_empty():
            if not skip_fetch_line_next:
                line=self.get_next_line()

            skip_fetch_line_next=False

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
                                current_line_filename=line[2].src_loc.filename

                                if current_line_filename not in self.files_included:
                                    self.files_included[current_line_filename]=Preprocessor.IncludeFileReference(current_line_filename,has_include_guard=True)
                                else:
                                    self.files_included[current_line_filename].has_include_guard=True

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
                                    global_include_str+=ind(tok.log_loc.col-col_index)
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
                        include_file_lines=sort_tokens_into_lines(include_file_tokens)

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
                                    if argument_name.s==VARARG_ARGNAME:
                                        define.has_vararg=True
                                        tok_index+=1
                                        break # vararg must be last argument

                                    define.arguments.append(argument_name)

                                    tok_index+=1 # skip over argument name
                                    if line[tok_index].s==",":
                                        tok_index+=1 # skip over comma

                                tok_index+=1 # skip over closing paranthesis

                                first_tok_index=tok_index

                            define.tokens.extend(line[first_tok_index:])

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
                        fatal(f"directive embed unimplemented!")

                    case unimplemented_directive:
                        fatal(f"directive {unimplemented_directive} unimplemented!")
            else:
                skip_parsing_lines=not self.get_if_state()

                expand_tokens:list[Token]=[]
                while line[0].s!="#":
                    if not skip_parsing_lines:
                        expand_tokens.extend(line)

                    if self.is_empty():
                        break

                    line=self.get_next_line()

                skip_fetch_line_next=True
                
                if not skip_parsing_lines:
                    new_line=self.expand(expand_tokens)

                    self.out_lines.append(new_line)

        return self.out_lines
