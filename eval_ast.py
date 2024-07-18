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

class TokenType(str,Enum):
    COMMENT="comment"
    SYMBOL="symbol"
    WHITESPACE="whitespace"
    GLOBAL_HEADER="global_header"
    # local header is just a string

    LITERAL_STRING="literal_string"
    LITERAL_CHAR="literal_char"
    LITERAL_NUMBER="literal_number"

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


def print_tokens(tokens,
    mode:tp.Literal["source","logical"]="source",
    filename="",
    ignore_comments:bool=False,
    ignore_whitespace:bool=False
):
    # print the tokens, with alternating text color
    token_index=0
    last_loc=SourceLocation("",-1,0)

    for line_num,line_tokens in enumerate(tokens):
        for token in line_tokens:
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
            print(f"-{color}{token.s}{RESET}-",end="")

            if mode=="source":
                last_loc.col=token.src_loc.col+len(token.s)
            elif mode=="logical":
                last_loc.col=token.log_loc.col+len(token.s)


def is_whitespace(c):
    return c in set([whitespace_char for whitespace_char in " \t\r\n"])

def is_special(c):
    return c in set([special_char for special_char in "(){}[]<>,.+-/*&|%^;:=?!'\""])

def is_numeric(c):
    return c in set([n for n in "0123456789"])

def main():
    if len(sys.argv)<2:
        print("no input file")
        return

    filename=sys.argv[1]
    code_file=io.open(filename)

    class Tokenizer:
        " utility class to convert file characters into tokens "

        def __init__(self,filename:str,lines_raw:tp.List[str]):
            self.filename=filename

            self.lines_raw=lines_raw
            self.line_index:int=-1
            self.col_index:int=0

            self.lines_tokenized:tp.List[tp.List[Token]]=[]

            self.merged_line_info:tp.List[tp.Tuple[int,int]]=[]

            self.current_line:str=""
            self.adv_line()

        @property
        def c(self):
            " return character at current pointer location in line "
            return self.current_line[self.col_index]

        def adv(self):
            " advance current line pointer to next character in line "
            self.col_index+=1

        @property
        def line_nc_rem(self)->int:
            " return number of characters left in line after current pointer "
            return len(self.current_line)-self.col_index-1

        def c_fut(self,n):
            " return character in current line n positions in advance of current pointer "
            return self.current_line[self.col_index+n]

        @property
        def line_empty(self):
            " returns True if current line as no characters left "
            if self.line_index>=len(self.lines_raw):
                return True

            if self.col_index>=len(self.current_line):
                return True

            return False

        def adv_line(self):
            " advance pointer to next line "
            self.line_index+=1
            self.col_index=0

            self.merged_line_info=[(self.line_index,0)]

            self.line_index=self.line_index
            if self.line_index>=len(self.lines_raw):
                return

            self.current_line=self.lines_raw[self.line_index]

            while 1:
                line_end_index=len(self.current_line)-1
                while line_end_index>=0 and is_whitespace(self.current_line[line_end_index]):
                    line_end_index-=1

                if line_end_index>=0 and self.current_line[line_end_index]=="\\":
                    self.line_index+=1

                    self.merged_line_info.append((self.line_index,line_end_index-1))

                    next_line=self.lines_raw[self.line_index]

                    self.current_line=self.current_line[:line_end_index]+next_line

                else:
                    break

        def current_loc(self):
            " return source location of current pointer into file "
            ret_line_index=self.merged_line_info[0][0]
            ret_col_index=self.col_index

            # current self.col_index is index into self.current_line
            # self.merged_line_info contains a list of start positions for the real line number
            for real_line_index,col_start in self.merged_line_info:
                if self.col_index>col_start and real_line_index>ret_line_index:
                    #fatal("really")
                    ret_col_index=self.col_index-col_start
                    ret_line_index=real_line_index

            return SourceLocation(self.filename,ret_line_index,ret_col_index)

        @property
        def remaining(self):
            " return True if any characters are remaining in the file "
            if self.line_index>=len(self.lines_raw):
                return False

            if self.col_index>=len(self.current_line):
                return False

            return True

        def add_tok_line(self):
            " add new token line "
            self.lines_tokenized.append([])

        def add_tok(self,token:Token):
            " add token to latest token line "
            self.lines_tokenized[-1].append(token)

    t=Tokenizer(filename,code_file.readlines())

    # tokenize the file (phase 3, combined with phase 2)
    while t.remaining:
        t.add_tok_line()

        # parse global header include #include<someheader>
        if 0:
            lstrip_line=t.current_line.lstrip()
            if lstrip_line.startswith("#"):
                after_hash_line=lstrip_line.split("#",1)[1].lstrip()
                INCLUDE_STR="include"
                if after_hash_line.startswith(INCLUDE_STR):
                    include_text=after_hash_line[len(INCLUDE_STR):].lstrip()
                    if include_text[0]=="<":
                        fatal("unimplemented")

        while not t.line_empty:
            current_token:Token=Token("",t.current_loc())

            skip_col_increment:bool=False

            while not t.line_empty:
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
                char_is_dot_followed_by_numeric=t.c=="." and t.line_nc_rem>0 and is_numeric(t.c_fut(1))
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

                    while not t.line_empty:
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
                            if not (t.line_nc_rem>0 and is_numeric(t.c_fut(1))):
                                fatal(f"digit separator cannot appear at end of digit sequence at {t.current_loc()}")

                            if parsed_exponent and num_exponent_digits==0:
                                fatal(f"digit separator cannot appear at start of digit sequence at {t.current_loc()}")

                            current_token.s+=t.c
                            t.adv()

                        else:
                            # append suffix until special char or whitespace
                            # TODO this must extend beyond the end of the line
                            while (not t.line_empty) and not (is_special(t.c) or is_whitespace(t.c)):
                                current_token.s+=t.c
                                t.adv()

                            skip_col_increment=True
                            break

                    if parsed_exponent:
                        if num_exponent_digits==0:
                            fatal(f"exponent has no digits at {t.current_loc()}")

                    break

                def parse_terminated_literal(start_char:str,end_char:str):
                    nonlocal current_token # type: Token
                    nonlocal skip_col_increment

                    if len(current_token.s)>0:
                        return False

                    if t.c==start_char:
                        if len(current_token.s)>0:
                            skip_col_increment=True
                            return True # break

                        current_token.token_type=TokenType.LITERAL_CHAR

                        current_token.s+=t.c
                        t.adv()

                        while 1:
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
                                skip_col_increment=True
                                break

                            t.adv()

                        return True # break

                    return False

                # parse a character literal
                if parse_terminated_literal("'","'"):
                    break

                # parse a string literal
                if parse_terminated_literal('"','"'):
                    break

                # parse a special symbol
                if is_special(t.c):
                    if len(current_token.s)>0:
                        skip_col_increment=True
                        break

                    # check for single line comment
                    if t.c=="/" and t.line_nc_rem>0 and t.c_fut(1)=="/":
                        current_token.token_type=TokenType.COMMENT

                        while not t.line_empty:
                            current_token.s+=t.c
                            t.adv()

                        break

                    # check for multi line comment
                    if t.c=="/" and t.line_nc_rem>0 and t.c_fut(1)=="*":
                        current_token.token_type=TokenType.COMMENT

                        current_token.s+=t.c
                        t.adv()

                        while 1:
                            current_token.s+=t.c

                            if len(current_token.s)>=4:
                                if current_token.s[-2:]=="*/":
                                    break

                            t.adv()
                            if t.line_empty:
                                t.adv_line()

                        break

                    # check for compound symbols (made up from more than one special symbol)
                    def compound_symbol_present(c_sym:str):
                        if t.c==c_sym[0] and t.line_nc_rem>=(len(c_sym)-1):
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

                    if compound_symbol_present("--"):
                        break
                    if compound_symbol_present("-="):
                        break
                    if compound_symbol_present("++"):
                        break
                    if compound_symbol_present("+="):
                        break
                    if compound_symbol_present("||"):
                        break
                    if compound_symbol_present("|="):
                        break
                    if compound_symbol_present("&&"):
                        break
                    if compound_symbol_present("&="):
                        break

                    if compound_symbol_present("^="):
                        break

                    if compound_symbol_present("!="):
                        break
                    if compound_symbol_present("=="):
                        break

                    if compound_symbol_present("<="):
                        break
                    if compound_symbol_present(">="):
                        break

                    if compound_symbol_present("<<"):
                        break
                    if compound_symbol_present(">>"):
                        break
                    if compound_symbol_present("<<="):
                        break
                    if compound_symbol_present(">>="):
                        break

                    if compound_symbol_present("->"):
                        break

                    current_token.s=t.c
                    break

                current_token.s+=t.c
                t.adv()

            if len(current_token.s)>0:
                if current_token.token_type==TokenType.COMMENT:
                    assert current_token.s[0]=="/", f"{current_token}"
                if current_token.token_type==TokenType.WHITESPACE:
                    assert current_token.is_whitespace, f"{current_token}"
                t.add_tok(current_token)

            if not skip_col_increment:
                t.adv()

        t.adv_line()

    # visually inspect results
    #print_tokens(t.lines_tokenized,mode="source",ignore_comments=True,ignore_whitespace=True)
    #print_tokens(t.lines_tokenized,mode="logical",ignore_comments=True,ignore_whitespace=True)

    # collapse tokens and remove whitespace and comments
    final_tokens=[]
    for line in t.lines_tokenized:
        for token in line:
            match token.token_type:
                case TokenType.COMMENT:
                    continue
                case TokenType.WHITESPACE:
                    continue
                case tok:
                    final_tokens.append(tok)

    return


if __name__=="__main__":
    main()