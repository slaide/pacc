import sys, io, time
from dataclasses import dataclass, field
import typing as tp
from enum import Enum
from pathlib import Path
import inspect
import re

from py_util import *
from py_tokenizer import *
from py_preprocessor import VARARG_ARGNAME

class CType:
    "basic c type class"
    def __init__(self,_is_basic:bool=True):
        self._is_basic=_is_basic
        "indicates if the type is a basic type, i.e. e.g. not pointer or array"

        self.is_static:bool=False
        self.is_extern:bool=False
        self.is_threadlocal:bool=False
        self.is_noreturn:bool=False

        self.is_atomic:bool=False

        self.length_mod:int|None=None
        self.is_const:bool=False

        self.is_signed:bool|None=None

        self.base_type:CType|None=None

    def copy(self)->"CType":
        ret=CType()
        for key,val in self.__dict__.items():
            setattr(ret,key,val)

        return ret

    def flatten(self)->"CType":
        "resolve basic nesting"

        if not self._is_basic:
            return self

        if self.base_type is None:
            return self

        return self.base_type.flatten()

    def nest(self):
        """
        turn self into new ctype with base set to previous self.

        can be used to turn self into another type, by creating a new type, nesting this, then setting base_type to new
        type based on previous self

        e.g. turn self into function with self as return type:
        ```
        ctype=CType()
        ctype... <modify>

        ctype.nest()

        func_type=CTypeFunction(return_type=ctype.base_type)
        ctype.base_type=func_type()
        ```
        """

        # copy self into inner_ctype
        inner_ctype=CType()
        for key,val in self.__dict__.items():
            setattr(inner_ctype,key,val)

        # clear self to defaults
        default_type=CType()
        for key,val in default_type.__dict__.items():
            setattr(self,key,val)

        self.base_type=inner_ctype

    def validate(self)->str|None:
        "return error message if type is invalid"
        if self.length_mod is not None:
            if self.length_mod < -2:
                return "type cannot be shorter than short short"
            if self.length_mod > 2:
                return "type cannot be longer than long long"

        if self.is_empty_default():
            return "type is basic and invalid"

        if self.base_type is not None:
            return self.base_type.validate()

        return None

    def can_be_assigned_to(self,other:"CType")->bool:
        "checks if self can be assigned to other"
        if isinstance(self,CTypePrimitive) and isinstance(other,CTypePrimitive):
            if self.name==other.name:
                return True

            if self.name==CTypePrimitive.Type().name and other.name==CTypePrimitive.Ty_Any().name:
                return True

        return False

    def is_empty_default(self)->bool:
        "returns True if the type has every field set to its default"
        return self.is_static==False and \
            self.is_extern==False and \
            self.is_atomic==False and \
            self.is_const==False and \
            self.is_signed==None and \
            self.length_mod==None and \
            self.base_type==None

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

        if self.length_mod is not None:
            match self.length_mod:
                case 2:
                    ret+="long long "
                case 1:
                    ret+="long "
                case -2:
                    ret+="short short "
                case -1:
                    ret+="short "
                case _:
                    pass

        return ret

    def print(self,indent:int):
        name_str=str(self)
        if len(name_str)>0:
            name_str+=" "

        print(ind(indent)+name_str,end="")
        if self.base_type is not None:
            print("base:")
            self.base_type.print(indent+1)
        else:
            print("")

    def get_field_by_name(self,field_name:str)->"CTypeField|None":
        if self.base_type is not None:
            return self.base_type.get_field_by_name(field_name)

        raise TypeError("type does not have any fields")

class CTypeField(CType):
    "information on a field of a compound type"
    def __init__(self,name:Token|None,ctype:CType,parent_ctype:CType):
        super().__init__(_is_basic=False)
        self.name=name
        "optional name of the field"
        self.ctype=ctype
        "type of the field"
        self.parent_ctype=parent_ctype
        "compound type which this field is part of"

    @tp.override
    def print(self,indent:int):
        print(ind(indent)+"field:")
        if self.name is None:
            print(ind(indent+1)+f"name: <anon>")
        else:
            print(ind(indent+1)+f"name: {self.name.s}")
        print(ind(indent+1)+f"type:")
        self.ctype.print(indent+2)

class CTypeStruct(CType):
    "name may be none, and if fields are none, the type is considered incomplete"
    def __init__(self,name:Token|None,fields:list[CTypeField]|None):
        super().__init__(_is_basic=False)
        self.name=name
        "optional name of this struct type"
        self.fields=fields
        "fields of the struct. may be none (no list present) on an incomplete type, e.g. forward declaration or opaque type"

    @property
    def is_incomplete(self)->bool:
        return self.fields is None

    @tp.override
    def is_empty_default(self)->bool:
        return False

    @tp.override
    def print(self,indent:int):
        print(ind(indent)+"struct:")
        if self.name is not None:
            print(ind(indent+1)+f"name: {self.name.s}")

        if self.fields is None:
            print(ind(indent+1)+f"fields: <type is incomplete>")
        else:
            print(ind(indent+1)+f"fields:")
            for field in self.fields:
                print(ind(indent+2)+"field:")
                if field.name is not None:
                    print(ind(indent+3)+f"name: {field.name.s}")
                print(ind(indent+3)+f"type:")
                field.ctype.print(indent+4)

    @tp.override
    def get_field_by_name(self,field_name:str)->"CTypeField|None":
        if self.fields is None:
            raise RuntimeError(f"type {self.name=} is incomplete, cannot have any types")

        for field in self.fields:
            if field.name is not None and field.name.s==field_name:
                return field

        return None

class CTypeUnion(CType):
    "name may be none, and if fields are none, the type is considered incomplete"
    def __init__(self,name:Token|None,fields:list[CTypeField]|None):
        super().__init__(_is_basic=False)
        self.name=name
        self.fields=fields

    @property
    def is_incomplete(self)->bool:
        return self.fields is None

    @tp.override
    def is_empty_default(self)->bool:
        return False

    @tp.override
    def print(self,indent:int):
        print(ind(indent)+"union:")
        if self.name is not None:
            print(ind(indent+1)+f"name: {self.name.s}")

        if self.fields is None:
            print(ind(indent+1)+f"fields: <type is incomplete>")
        else:
            print(ind(indent+1)+f"fields:")
            for field in self.fields:
                print(ind(indent+2)+"field:")
                if field.name is not None:
                    print(ind(indent+3)+f"name: {field.name.s}")
                print(ind(indent+3)+f"type:")
                field.ctype.print(indent+4)

    @tp.override
    def get_field_by_name(self,field_name:str)->"CTypeField|None":
        if self.fields is None:
            raise RuntimeError("type is incomplete, cannot have any types")

        for field in self.fields:
            if field.name is not None and field.name.s==field_name:
                return field

        return None

class CTypeEnum(CType):
    "name may be none, and if fields are none, the type is considered incomplete"
    def __init__(self,base_type:CType,name:Token|None,fields:list[tuple["Symbol","AstValue"]]|None):
        super().__init__(_is_basic=False)

        self.base_type=base_type
        self.name=name
        self.fields=fields

    @property
    def is_incomplete(self)->bool:
        return self.fields is None

    @tp.override
    def is_empty_default(self)->bool:
        return False

    @tp.override
    def print(self,indent:int):
        print(ind(indent)+"union:")
        if self.name is not None:
            print(ind(indent+1)+f"name: {self.name.s}")

        if self.fields is None:
            print(ind(indent+1)+f"fields: <type is incomplete>")
        else:
            print(ind(indent+1)+f"fields:")
            for field,value in self.fields:
                print(ind(indent+2)+"field:")
                assert field.name is not None
                print(ind(indent+3)+f"name: {field.name.s}")
                print(ind(indent+3)+"value:")
                value.print(indent+4)


class CTypePrimitive(CType):
    def __init__(self,name:str):
        super().__init__(_is_basic=False)
        self.name=name
        assert len(self.name)>0

    @tp.override
    def is_empty_default(self)->bool:
        return False

    @tp.override
    def print(self,indent:int):
        print(ind(indent)+self.name)

    # fundamental types:

    @staticmethod
    def Void():
        return CTypePrimitive("void")
    @staticmethod
    def Int():
        return CTypePrimitive("int")
    @staticmethod
    def Char():
        return CTypePrimitive("char")
    @staticmethod
    def Float():
        return CTypePrimitive("float")
    @staticmethod
    def Double():
        return CTypePrimitive("double")
    @staticmethod
    def Bool():
        return CTypePrimitive("bool")

    @staticmethod
    def Type():
        return CTypePrimitive("__type")
    @staticmethod
    def Ty_Any():
        return CTypePrimitive("__ty_any")
    @staticmethod
    def Any():
        return CTypePrimitive("__any")

class CTypePointer(CType):
    "pointer to some other type (uses CType.base_type as base)"
    def __init__(self,base_type:CType):
        super().__init__(_is_basic=False)
        self.base_type=base_type

    @tp.override
    def is_empty_default(self)->bool:
        return False

    @tp.override
    def print(self,indent:int):
        assert self.base_type is not None
        print(ind(indent)+"ptr to:")
        self.base_type.print(indent+1)

    @tp.override
    def validate(self)->str|None:
        if self.base_type is None:
            return "pointer to nothing is invalid"

        return self.base_type.validate()

class CTypeArray(CType):
    def __init__(self,base_type:CType,length:"AstValue|None",length_is_static:bool=False):
        super().__init__(_is_basic=False)
        self.base_type=base_type
        self.length=length

        self.length_is_static=length_is_static

    @tp.override
    def is_empty_default(self)->bool:
        return False

    @tp.override
    def print(self,indent:int):
        print(ind(indent)+"array:")
        print(ind(indent+1)+"base_type:")
        assert self.base_type is not None
        self.base_type.print(indent+2)
        if self.length is None:
            print(ind(indent+1)+"length: <undefined>")
        else:
            print(ind(indent+1)+"length:")
            self.length.print(indent+2)

class CTypeFunction(CType):
    "function type"
    def __init__(self,return_type:CType,arguments:list["Symbol"],has_vararg:bool=False):
        super().__init__(_is_basic=False)
        self.return_type=return_type
        self.arguments=arguments
        self.has_vararg=has_vararg

    @tp.override
    def is_empty_default(self)->bool:
        return False

    @tp.override
    def print(self,indent:int):
        print(ind(indent)+f"fn:")
        print(ind(indent+1)+"args:")
        for arg in self.arguments:
            if arg.name is None:
                print(ind(indent+2)+f"<anon>:")
                arg.ctype.print(indent+3)
            else:
                print(ind(indent+2)+f"{arg.name.s}:")
                arg.ctype.print(indent+3)

        print(ind(indent+1)+"ret:")
        self.return_type.print(indent+2)

    @tp.override
    def validate(self)->str|None:
        "return error message if type is invalid"
        ret_val_str=self.return_type.validate()
        if ret_val_str is not None:
            return ret_val_str

        for arg in self.arguments:
            arg_val_str=arg.ctype.validate()
            if arg_val_str is not None:
                return arg_val_str

        return None

class CTypeConstFn(CTypeFunction):
    "compile time function, allows operating on the AST level. self.eval contains the actual function code"
    def __init__(self,return_kind:"type[CType]|type[AstValue]",arguments:list[tuple[str,"CType"]]):
        super().__init__(return_type=CTypePrimitive("mock"),arguments=[Symbol(t) for _,t in arguments])

    def eval(self,block:"Block",arguments:list["AstValue"])->"AstValue":
        fatal("must be overridden")

class Symbol:
    def __init__(self,ctype:CType,name:Token|None=None):
        self.name=name
        self.ctype:CType=ctype

    def wrap_ctype(self,wrapped_by:tp.Callable[[CType],CType]):
        "wrap the current symbol type by a new type, e.g. by an array.\nself.ctype will then be set to the new type"
        wrapping_ctype=wrapped_by(self.ctype)
        self.ctype=wrapping_ctype

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

    def get_ctype(self)->"CType":
        raise RuntimeError("raw AstValue cannot have a type")

class AstValueType(AstValue):
    def __init__(self,ctype:CType):
        super().__init__()
        self.ctype=ctype

    @tp.override
    def print(self,indent:int):
        print(ind(indent)+"type as value:")
        self.ctype.print(indent+1)

    @tp.override
    def get_ctype(self)->"CType":
        return CTypePrimitive("__type")

class AstValueField(AstValue):
    "operator value to access a field of a value, i.e. this is ensured to exist and the type is resolved"
    def __init__(self,field:CTypeField):
        super().__init__()
        self.field=field

    @tp.override
    def get_ctype(self)->"CType":
        return self.field.ctype

class AstCompoundLiteral(AstValue):
    "compound value initializer, e.g. struct Mystruct m={.field=2,3,'hello world'};"
    def __init__(self,field_initializers:list[tuple[AstValueField|None,AstValue]]):
        super().__init__()
        self.field_initializers=field_initializers

    @tp.override
    def print(self,indent:int):
        print(ind(indent)+"compound literal:")
        for field_name,field_value in self.field_initializers:
            print(ind(indent+1)+"field:")

            if field_name is not None:
                print(ind(indent+2)+"target:")
                field_name.print(indent+3)

            print(ind(indent+2)+"value:")
            field_value.print(indent+3)


class AstValueNumericLiteral(AstValue):
    def __init__(self,value:str|int|float,ctype:CType):
        super().__init__()
        self.value=value
        self.ctype:CType=ctype

    @tp.override
    def print(self,indent:int):
        print(ind(indent)+"number:")
        print(ind(indent+1)+"literal: "+repr(self.value))
        print(ind(indent+1)+"type: ")
        self.ctype.print(indent+2)

    @tp.override
    def get_ctype(self)->"CType":
        return self.ctype

class AstValueSymbolref(AstValue):
    def __init__(self,symbol_to_ref:Symbol):
        super().__init__()
        self.symbol=symbol_to_ref

    @tp.override
    def print(self,indent:int):
        assert self.symbol.name is not None, "should be unreachable"
        print(ind(indent)+"ref sym: "+self.symbol.name.s)

    @tp.override
    def get_ctype(self)->"CType":
        return self.symbol.ctype

class AstFunctionCall(AstValue):
    def __init__(self,func:AstValue,arguments:list[AstValue]):
        super().__init__()
        self.func=func
        self.arguments=arguments

    @tp.override
    def print(self,indent:int):
        print(ind(indent)+"call:")
        self.func.print(indent+1)

        print(ind(indent+1)+"args:")
        for arg in self.arguments:
            arg.print(indent+2)

    @tp.override
    def get_ctype(self)->"CType":
        func_type=self.func.get_ctype()
        if not isinstance(func_type,CTypeFunction):
            raise RuntimeError("should not happen, attempting to call non-function")

        return func_type.return_type

class AstOperationKind(str,Enum):
    GREATER_THAN=">"
    GREATER_THAN_OR_EQUAL=">="
    LESS_THAN="<"
    LESS_THAN_OR_EQUAL="<="

    LOGICAL_NOT="!"
    BITWISE_NOT="~"

    LOGICAL_AND="&&"
    BITWISE_AND="&"
    LOGICAL_OR="||"
    BITWISE_OR="|"

    BITWISE_XOR="^"

    ADD="+"
    SUBTRACT="-"

    UNARY_PLUS="+x"
    UNARY_MINUS="-x"

    MULTIPLY="*"
    DIVIDE="/"
    MODULO="%"

    EQUAL="=="
    UNEQUAL="!="

    ADD_ASSIGN="+="
    SUB_ASSIGN="-="
    DIV_ASSIGN="/="
    MULT_ASSIGN="*="
    MOD_ASSIGN="%="
    XOR_ASSIGN="^="
    BITWISE_OR_ASSIGN="|="
    BITWISE_AND_ASSIGN="&="

    ASSIGN="="
    DOT="."
    ARROW="->"

    ADDROF="&x"
    DEREFERENCE="*x"

    QMARK="?:"

    POSTFIX_INCREMENT="x++"
    POSTFIX_DECREMENT="x--"
    PREFIX_INCREMENT="++x"
    PREFIX_DECREMENT="--x"

    SUBSCRIPT="["
    NESTED="("
    CAST="(t)"

    @property
    def precedence(self)->int:
        "returns precedence. operands for operators with lower precedence are parsed until an operator with same or higher precedence is encountered"
        match self:
            case AstOperationKind.POSTFIX_INCREMENT:
                return 1
            case AstOperationKind.POSTFIX_DECREMENT:
                return 1
            case AstOperationKind.NESTED:
                return 1
            case AstOperationKind.SUBSCRIPT:
                return 1
            case AstOperationKind.DOT:
                return 1
            case AstOperationKind.ARROW:
                return 1

            case AstOperationKind.PREFIX_INCREMENT:
                return 2
            case AstOperationKind.PREFIX_DECREMENT:
                return 2
            case AstOperationKind.UNARY_MINUS:
                return 2
            case AstOperationKind.UNARY_PLUS:
                return 2
            case AstOperationKind.LOGICAL_NOT:
                return 2
            case AstOperationKind.BITWISE_NOT:
                return 2
            case AstOperationKind.CAST:
                return 2
            case AstOperationKind.DEREFERENCE:
                return 2
            case AstOperationKind.ADDROF:
                return 2

            case _:
                fatal(f"unimplemented {self}")


class AstOperation(AstValue):
    def __init__(self,operation:str|AstOperationKind,val0:AstValue,val1:AstValue|None=None,val2:AstValue|None=None):
        super().__init__()
        self.operation=AstOperationKind(operation)
        self.val0=val0
        self.val1=val1
        self.val2=val2

    @tp.override
    def print(self,indent:int):
        def print_op(num_args:int):
            print(ind(indent)+f"op: {self.operation.value}")
            self.val0.print(indent+1)

            match num_args:
                case 1:
                    assert self.val1 is None
                    assert self.val2 is None
                case 2:
                    assert self.val1 is not None
                    self.val1.print(indent+1)
                    assert self.val2 is None
                case 3:
                    assert self.val1 is not None
                    self.val1.print(indent+1)
                    assert self.val2 is not None
                    self.val2.print(indent+1)
                case _:
                    fatal("unreachable")

        match self.operation:
            case AstOperationKind.GREATER_THAN:
                print_op(2)
            case AstOperationKind.GREATER_THAN_OR_EQUAL:
                print_op(2)
            case AstOperationKind.LESS_THAN:
                print_op(2)
            case AstOperationKind.LESS_THAN_OR_EQUAL:
                print_op(2)

            case AstOperationKind.EQUAL:
                print_op(2)
            case AstOperationKind.UNEQUAL:
                print_op(2)

            case AstOperationKind.LOGICAL_AND:
                print_op(2)
            case AstOperationKind.LOGICAL_OR:
                print_op(2)
            case AstOperationKind.BITWISE_AND:
                print_op(2)
            case AstOperationKind.BITWISE_OR:
                print_op(2)

            case AstOperationKind.ARROW:
                print_op(2)
            case AstOperationKind.DOT:
                print_op(2)

            case AstOperationKind.ASSIGN:
                print_op(2)
            case AstOperationKind.ADD_ASSIGN:
                print_op(2)
            case AstOperationKind.SUB_ASSIGN:
                print_op(2)

            case AstOperationKind.POSTFIX_DECREMENT:
                print_op(1)
            case AstOperationKind.POSTFIX_INCREMENT:
                print_op(1)
            case AstOperationKind.PREFIX_DECREMENT:
                print_op(1)
            case AstOperationKind.PREFIX_INCREMENT:
                print_op(1)

            case AstOperationKind.ADD:
                print_op(2)
            case AstOperationKind.SUBTRACT:
                print_op(2)
            case AstOperationKind.MULTIPLY:
                print_op(2)
            case AstOperationKind.DIVIDE:
                print_op(2)
            case AstOperationKind.MODULO:
                print_op(2)

            case AstOperationKind.UNARY_MINUS:
                print_op(1)
            case AstOperationKind.UNARY_PLUS:
                print_op(1)

            case AstOperationKind.ADDROF:
                print_op(1)
            case AstOperationKind.DEREFERENCE:
                print_op(1)

            case AstOperationKind.LOGICAL_NOT:
                print_op(1)
            case AstOperationKind.BITWISE_NOT:
                print_op(1)

            case AstOperationKind.SUBSCRIPT:
                print_op(2)

            case AstOperationKind.QMARK:
                print_op(3)

            case other:
                fatal(f"unimplemented {other}")

    @tp.override
    def get_ctype(self)->"CType":
        match self.operation:
            case AstOperationKind.DOT:
                assert self.val1 is not None
                return self.val1.get_ctype()

            case AstOperationKind.ARROW:
                assert self.val1 is not None
                return self.val1.get_ctype()

            case AstOperationKind.POSTFIX_DECREMENT:
                return self.val0.get_ctype()
            case AstOperationKind.POSTFIX_INCREMENT:
                return self.val0.get_ctype()
            case AstOperationKind.PREFIX_DECREMENT:
                return self.val0.get_ctype()
            case AstOperationKind.PREFIX_INCREMENT:
                return self.val0.get_ctype()

            case _:
                # TODO actually calculate this approriately, e.g. taking implicit conversion into account, and overloaded operations like pointer arithmetic
                return self.val0.get_ctype()

class AstOperationCast(AstOperation):
    def __init__(self,cast_to_type:CType,value:AstValue):
        super().__init__(operation=AstOperationKind.CAST,val0=value)
        self.cast_to_type=cast_to_type

    @tp.override
    def print(self,indent:int):
        print(ind(indent)+"cast:")
        print(ind(indent+1)+"value:")
        self.val0.print(indent+2)
        print(ind(indent+1)+"to type:")
        self.cast_to_type.print(indent+2)

    @tp.override
    def get_ctype(self)->"CType":
        return self.cast_to_type

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
        p=ind(indent)+"return"
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
        print(ind(indent)+"value:")
        self.value.print(indent+1)

class StatementTypedef(Statement):
    def __init__(self,*symbols:Symbol):
        super().__init__()
        self.symbols=list(symbols)

    @tp.override
    def print(self,indent:int):
        print(ind(indent)+"typedef:")
        for sym in self.symbols:
            if sym.name is None:
                print(ind(indent+1)+f"alias <anon>")
            else:
                print(ind(indent+1)+f"alias {sym.name.s}")
            sym.ctype.print(indent+2)


class SymbolDef(Statement):
    "statement symbol definition"
    def __init__(self,symbols:list[tuple[Symbol,"AstValue|None"]]):
        super().__init__()
        self.symbols=symbols

    @property
    def num_sym(self)->int:
        return len(self.symbols)

    @tp.override
    def print(self,indent:int):
        print(ind(indent)+"def:")
        for sym,val in self.symbols:
            if sym.name is None:
                print(ind(indent+1)+f"<anon>:")
            else:
                print(ind(indent+1)+f"{sym.name.s}:")

            sym.ctype.print(indent+2)

            if val is not None:
                print(ind(indent+2)+"value:")
                val.print(indent+3)

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

    def getTypeByName(self,name:str,e_struct:bool=False,e_enum:bool=False,e_union:bool=False)->CType|None:
        typename_is_explicit_flag=sum((int(v) for v in(e_struct,e_enum,e_union)))
        assert typename_is_explicit_flag<=1

        typename_is_explicit=typename_is_explicit_flag==1

        if typename_is_explicit:
            if e_struct and name in self.struct_types:
                return self.struct_types[name]
            elif e_enum and name in self.enum_types:
                return self.enum_types[name]
            elif e_union and name in self.union_types:
                return self.union_types[name]

        elif name in self.types:
            return self.types[name]

        if self.parent is not None:
            return self.parent.getTypeByName(name,e_struct=e_struct,e_enum=e_enum,e_union=e_union)

        return None

    def getSymbolByName(self,name:str)->Symbol|None:
        if name in self.symbols:
            return self.symbols[name]

        if self.parent is not None:
            return self.parent.getSymbolByName(name)

        return None

    def addType(self,ctype:CType):
        "make type public, intended for use in symbol definitions to publicize a symbol type"
        if isinstance(ctype,CTypeStruct):
            if ctype.name is not None:
                self.struct_types[ctype.name.s]=ctype

        elif isinstance(ctype,CTypeUnion):
            if ctype.name is not None:
                self.union_types[ctype.name.s]=ctype

        elif isinstance(ctype,CTypeEnum):
            if ctype.name is not None:
                self.enum_types[ctype.name.s]=ctype

            # also ingest every 'field' of the enum into the symbol table
            if ctype.fields is not None:
                for field,_ in ctype.fields:
                    self.addSymbol(field)

        if ctype.base_type is not None:
            self.addType(ctype.base_type)

    def addStatement(self,statement:Statement,ingest_symbols:bool=True):
        if ingest_symbols:
            # if statement is a symboldef, ingest every symbol defined there
            if isinstance(statement,SymbolDef):
                for sym,_ in statement.symbols:
                    self.addSymbol(sym)

            # if statement is a typedef, ingest all symbols and types defined there
            elif isinstance(statement,StatementTypedef):
                for sym in statement.symbols:
                    # ingest the referenced type first
                    self.addType(sym.ctype)

                    # then add the alias to the list of types
                    if sym.name is not None:
                        self.types[sym.name.s]=sym.ctype

        self.statements.append(statement)

    def addSymbol(self,symbol:Symbol):
        self.addType(symbol.ctype)

        if symbol.name is not None:
            self.symbols[symbol.name.s]=symbol

    def parse(self,t:Iter[Token])->Iter[Token]:
        "parse tokens as a series of statements into self.block, stop when something that is not valid syntax is encountered"
        while not t.empty:
            t,statement=self.parse_statement(t)
            if statement is None:
                break

            if isinstance(statement,AstFunction):
                self.addStatement(statement,ingest_symbols=False)
            else:
                self.addStatement(statement)

        return t

    def parse_statement(self,t:Iter[Token])->tuple[Iter[Token],Statement|None]:
        """
        parse a terminated statement
        """

        match t[0].s:
            case ";":
                t+=1
                return t,AstEmptyStatement()

            case "{":
                t+=1
                
                astblock=AstBlock(parent=self)
                t=astblock.parse(t)

                assert t[0].s=="}", f"got instead {t[0]}"
                t+=1

                return t,astblock

            case "typedef":
                t+=1

                t_aftertdef,typedef=self.parse_symbol_definition(t)
                
                if typedef is not None:
                    t=t_aftertdef

                assert t[0].s==";", f"got instead {t[0]}"
                t+=1

                if typedef is None:
                    return t,StatementTypedef()

                for _,val in typedef.symbols:
                    if val is not None:
                        fatal("assigning value in typedef statement is not allowed")

                return t,StatementTypedef(*[s for s,_ in typedef.symbols])

            case "switch":
                t+=1

                assert t[0].s=="(", f"got instead {t[0]}"
                t+=1

                t_after_cond,cond=self.parse_value(t)
                if cond is None:
                    fatal(f"invalid switch value at {t[0]}")

                t=t_after_cond

                assert t[0].s==")", f"got instead {t[0]}"
                t+=1

                t_after_statement,body_stmt=self.parse_statement(t)
                if body_stmt is None:
                    fatal(f"invalid switch body at {t[0]}")

                t=t_after_statement

                return t,AstSwitch(cond,body_stmt)

            case "case":
                t+=1

                t_after_val,val=self.parse_value(t)
                if val is None:
                    fatal(f"invalid case value at {t[0]}")

                t=t_after_val

                assert t[0].s==":"
                t+=1

                return t,AstSwitchCase(val)

            case "break":
                t+=1
                assert t[0].s==";"
                t+=1

                return t,AstLoopBreak()

            case "continue":
                t+=1
                assert t[0].s==";"
                t+=1

                return t,AstLoopContinue()

            case "default":
                t+=1
                assert t[0].s==":"
                t+=1

                return t,AstSwitchDefault()

            case "if":
                t+=1

                assert t[0].s=="(", f"got instead {t[0]}"
                t+=1

                t_after_cond,if_cond=self.parse_value(t)
                if if_cond is None:
                    fatal(f"invalid if condition at {t[0]}")

                t=t_after_cond

                assert t[0].s==")", f"got instead {t[0]}"
                t+=1

                t_after_statement,if_statement=self.parse_statement(t)
                if if_statement is None:
                    fatal(f"invalid if body at {t[0]}")

                t=t_after_statement

                else_statement:Statement|None=None
                if t[0].s=="else":
                    t+=1
                    t_after_else,else_statement=self.parse_statement(t)
                    if else_statement is None:
                        fatal(f"invalid else statment at {t[0]}")

                    t=t_after_else

                return t,AstIf(if_cond,if_statement,else_statement)

            case "do":
                t+=1

                t_after_statement,while_statement=self.parse_statement(t)
                if while_statement is None:
                    fatal(f"invalid do while body at {t[0]}")

                t=t_after_statement

                assert t[0].s=="while"
                t+=1

                assert t[0].s=="(", f"got instead {t[0]}"
                t+=1

                t_after_cond,cond=self.parse_value(t)
                if cond is None:
                    fatal(f"invalid do while condition at {t[0]}")

                t=t_after_cond

                assert t[0].s==")", f"got instead {t[0]}"
                t+=1

                return t,AstWhileLoop(cond,while_statement,do_while=True)

            case "while":
                t+=1

                assert t[0].s=="(", f"got instead {t[0]}"
                t+=1

                t_after_cond,cond=self.parse_value(t)
                if cond is None:
                    fatal(f"invalid while condition at {t[0]}")

                t=t_after_cond

                assert t[0].s==")", f"got instead {t[0]}"
                t+=1

                t_after_statement,while_statement=self.parse_statement(t)
                if while_statement is None:
                    fatal(f"invalid while body at {t[0]}")

                t=t_after_statement

                return t,AstWhileLoop(cond,while_statement)

            case "for":
                t+=1

                assert t[0].s=="(", f"got instead {t[0]}"
                t+=1

                # parse for init
                t,init_statement=self.parse_statement(t)
                assert init_statement is not None, f"got instead {t[0]}"

                for_block=AstForLoop(init_statement,parent=self)

                # parse for condition
                t,condition=for_block.parse_value(t)
                for_block.condition=condition

                assert t[0].s==";", f"got instead {t[0]}"
                t+=1

                # parse for step
                t,step_statement=for_block.parse_value(t)
                for_block.step=step_statement

                assert t[0].s==")", f"got instead {t[0]}"
                t+=1

                t,for_body_statement=for_block.parse_statement(t)
                assert for_body_statement is not None, f"got instead {t[0]}"

                for_block.addStatement(for_body_statement)

                return t,for_block

            case "return":
                t+=1

                t,value=self.parse_value(t)

                assert t[0].s==";", f"expected ; got instead {t[0]}"
                t+=1

                return t,StatementReturn(value)

            case "goto":
                t+=1

                assert t[0].token_type==TokenType.SYMBOL
                label_name=t[0]
                t+=1

                assert t[0].s==";"
                t+=1

                return t,AstGoto(label_name)

            case _:
                t,symbol_def=self.parse_symbol_definition(t)
                if symbol_def is not None:
                    for sym,_ in symbol_def.symbols:
                        self.addSymbol(sym)

                        sym_type=sym.ctype.flatten()
                        if isinstance(sym_type,CTypeFunction):
                            if t[0].s=="{":
                                t,func_def=self.parse_function_definition(func_type=sym_type,t=t)
                                if func_def is None:
                                    print(f"{RED}-- begin error {RESET}")
                                    sym.ctype.print(0)
                                    fatal(f"failed to parse function definition for decl at {sym.name.s if sym.name is not None else '<anon>'}")

                                return t,func_def

                    assert t[0].s==";", f"got instead {t[0]}"
                    t+=1

                    return t,symbol_def

                # check for label definition
                if t[0].token_type==TokenType.SYMBOL and t[1].s==":" and self.getSymbolByName(t[0].s) is None:
                    label=t[0]

                    t+=2

                    return t,AstLabel(label)

                t,value=self.parse_value(t)
                if value is not None:
                    assert t[0].s==";", f"got instead {t[0]}"
                    t+=1

                    return t,StatementValue(value)

                t,symdef=self.parse_symbol_definition(t)
                if symdef is not None:
                    assert t[0].s==";", f"got instead {t[0]}"
                    t+=1

                    return t,symdef

        return t,None

    def parse_value(self,t:Iter[Token],target_type:CType|None=None)->tuple[Iter[Token],AstValue|None]:
        "parse a value. target_type is primarily intended for compound literal initializers to allow named field access."

        ret=None
        while 1:
            match t[0].token_type:
                case TokenType.LITERAL_CHAR:
                    if ret is not None: break

                    chartype=self.getTypeByName("char")
                    assert chartype is not None
                    ret=AstValueNumericLiteral(t[0].s,chartype)
                    t+=1

                case TokenType.LITERAL_NUMBER:
                    if ret is not None: break
                    
                    # TODO actually decide the type of the numeric
                    numtype=self.getTypeByName("int")
                    assert numtype is not None
                    ret=AstValueNumericLiteral(t[0].s,numtype)
                    t+=1

                case TokenType.LITERAL_STRING:
                    if ret is not None: break
                    
                    # TODO actually decide the type of the numeric
                    cstring_type=self.getTypeByName("char")
                    assert cstring_type is not None
                    cstring_type=CTypePointer(cstring_type)
                    ret=AstValueNumericLiteral(t[0].s,cstring_type)
                    t+=1

                case TokenType.SYMBOL:
                    if ret is not None: break

                    match t[0].s:
                        case "false":
                            t+=1
                            ret=AstValueNumericLiteral(0,CTypePrimitive("bool"))
                        case "true":
                            t+=1
                            ret=AstValueNumericLiteral(1,CTypePrimitive("bool"))

                        case _:
                            pass

                    symbol=self.getSymbolByName(t[0].s)
                    if symbol is None:
                        break

                    ret=AstValueSymbolref(symbol)
                    t+=1

                case TokenType.OPERATOR_PUNCTUATION:
                    match t[0].s:
                        case "+":
                            t+=1

                            if ret is None:
                                t,ret=self.parse_value(t)
                                if ret is None: break
                                ret=AstOperation(AstOperationKind.UNARY_PLUS,ret)
                                continue

                            t,rhv=self.parse_value(t)
                            if rhv is None: break
                            ret=AstOperation(AstOperationKind.ADD,ret,rhv)

                        case "-":
                            t+=1

                            if ret is None:
                                t,ret=self.parse_value(t)
                                if ret is None: break
                                ret=AstOperation(AstOperationKind.UNARY_MINUS,ret)
                                continue

                            t,rhv=self.parse_value(t)
                            if rhv is None: break
                            ret=AstOperation(AstOperationKind.SUBTRACT,ret,rhv)

                        case "*":
                            t+=1

                            if ret is None:
                                t,ret=self.parse_value(t)
                                if ret is None: break
                                ret=AstOperation(AstOperationKind.DEREFERENCE,ret)
                                continue

                            t,rhv=self.parse_value(t)
                            if rhv is None: break
                            ret=AstOperation(AstOperationKind.MULTIPLY,ret,rhv)

                        case "/":
                            if ret is None: break

                            t+=1

                            t,rhv=self.parse_value(t)
                            if rhv is None: break
                            ret=AstOperation(AstOperationKind.DIVIDE,ret,rhv)

                        case "%":
                            if ret is None: break

                            t+=1

                            t,rhv=self.parse_value(t)
                            if rhv is None: break
                            ret=AstOperation(AstOperationKind.MODULO,ret,rhv)

                        case "!":
                            if ret is not None: break

                            t+=1

                            t,rhv=self.parse_value(t)
                            if rhv is None: break
                            ret=AstOperation(AstOperationKind.LOGICAL_NOT,rhv)

                        case "~":
                            if ret is not None: break

                            t+=1

                            t,rhv=self.parse_value(t)
                            if rhv is None: break
                            ret=AstOperation(AstOperationKind.BITWISE_NOT,rhv)

                        case "&":
                            t+=1

                            if ret is not None:
                                t,rhv=self.parse_value(t)
                                if rhv is None: break
                                ret=AstOperation(AstOperationKind.BITWISE_AND,ret,rhv)
                                break

                            t,rhv=self.parse_value(t)
                            if rhv is None: break
                            ret=AstOperation(AstOperationKind.ADDROF,rhv)

                        case ".":
                            t+=1

                            if ret is None:
                                break

                            assert t[0].token_type==TokenType.SYMBOL, f"got instead {t[0]}"

                            ret_ctype=ret.get_ctype()
                            if not (isinstance(ret_ctype,CTypeUnion) or isinstance(ret_ctype,CTypeStruct)):
                                print(f"{RED}error in type:{RESET}")
                                ret_ctype.print(0)
                                fatal(f"cannot use dot operator on non-[struct|union] type")

                            val_field=ret_ctype.get_field_by_name(t[0].s)
                            if val_field is None:
                                fatal(f"no field {t[0].s} in type {ret_ctype.name.s}")

                            t+=1

                            ret=AstOperation(AstOperationKind.DOT,ret,val_field) #type:ignore

                        case "->":
                            t+=1

                            if ret is None:
                                break

                            assert t[0].token_type==TokenType.SYMBOL

                            val_field=ret.get_ctype().get_field_by_name(t[0].s) #type:ignore
                            assert val_field is not None, f"got instead {t[0]}"

                            t+=1

                            ret=AstOperation(AstOperationKind.ARROW,ret,val_field) #type:ignore

                        case "[":
                            if ret is None:break

                            t+=1

                            t,index_value=self.parse_value(t)
                            if index_value is None:
                                fatal(f"invalid index at {t[0]}")

                            assert t[0].s=="]", f"got instead {t[0]}"
                            t+=1

                            ret=AstOperation(AstOperationKind.SUBSCRIPT,ret,index_value)

                        case "?":
                            if ret is None: break

                            t+=1

                            t,rhv0=self.parse_value(t)
                            if rhv0 is None: break

                            assert t[0].s==":", f"got instead {t[0]}"
                            t+=1

                            t,rhv1=self.parse_value(t)
                            if rhv1 is None: break

                            ret=AstOperation(AstOperationKind.QMARK,ret,rhv0,rhv1)

                        case "<":
                            if ret is None: break

                            t+=1

                            t,rhv=self.parse_value(t)
                            if rhv is None: break
                            ret=AstOperation(AstOperationKind.LESS_THAN,ret,rhv)

                        case "<=":
                            if ret is None: break

                            t+=1

                            t,rhv=self.parse_value(t)
                            if rhv is None: break
                            ret=AstOperation(AstOperationKind.LESS_THAN_OR_EQUAL,ret,rhv)

                        case ">":
                            if ret is None: break

                            t+=1

                            t,rhv=self.parse_value(t)
                            if rhv is None: break
                            ret=AstOperation(AstOperationKind.GREATER_THAN,ret,rhv)

                        case ">=":
                            if ret is None: break

                            t+=1

                            t,rhv=self.parse_value(t)
                            if rhv is None: break
                            ret=AstOperation(AstOperationKind.GREATER_THAN_OR_EQUAL,ret,rhv)

                        case "==":
                            if ret is None: break

                            t+=1

                            t,rhv=self.parse_value(t)
                            if rhv is None: break
                            ret=AstOperation(AstOperationKind.EQUAL,ret,rhv)

                        case "!=":
                            if ret is None: break

                            t+=1

                            t,rhv=self.parse_value(t)
                            if rhv is None: break
                            ret=AstOperation(AstOperationKind.UNEQUAL,ret,rhv)

                        case "&&":
                            if ret is None: break

                            t+=1

                            t,rhv=self.parse_value(t)
                            if rhv is None: break
                            ret=AstOperation(AstOperationKind.LOGICAL_AND,ret,rhv)

                        case "&":
                            if ret is None: break

                            t+=1

                            t,rhv=self.parse_value(t)
                            if rhv is None: break
                            ret=AstOperation(AstOperationKind.BITWISE_AND,ret,rhv)

                        case "||":
                            if ret is None: break

                            t+=1

                            t,rhv=self.parse_value(t)
                            if rhv is None: break
                            ret=AstOperation(AstOperationKind.LOGICAL_OR,ret,rhv)

                        case "|":
                            if ret is None: break

                            t+=1

                            t,rhv=self.parse_value(t)
                            if rhv is None: break
                            ret=AstOperation(AstOperationKind.BITWISE_OR,ret,rhv)

                        case "^":
                            if ret is None: break

                            t+=1

                            t,rhv=self.parse_value(t)
                            if rhv is None: break
                            ret=AstOperation(AstOperationKind.BITWISE_XOR,ret,rhv)

                        case "++":
                            t+=1

                            if ret is not None:
                                ret=AstOperation(AstOperationKind.POSTFIX_INCREMENT,ret)
                                continue

                            t,ret=self.parse_value(t)

                            if ret is None: break

                            ret=AstOperation(AstOperationKind.PREFIX_INCREMENT,ret)

                        case "--":
                            t+=1

                            if ret is not None:
                                ret=AstOperation(AstOperationKind.POSTFIX_DECREMENT,ret)
                                continue

                            t,ret=self.parse_value(t)

                            if ret is None: break

                            ret=AstOperation(AstOperationKind.PREFIX_DECREMENT,ret)

                        case "=":
                            if ret is None: break

                            t+=1

                            t_afterval,rhv=self.parse_value(t)
                            if rhv is None:
                                fatal(f"invalid value on rhs of assignment at {t[0]}")

                            t=t_afterval

                            ret=AstOperation(AstOperationKind.ASSIGN,ret,rhv)

                        case "+=":
                            if ret is None: break

                            t+=1

                            t_afterval,rhv=self.parse_value(t)
                            if rhv is None:
                                fatal(f"invalid value on rhs of assignment at {t[0]}")

                            t=t_afterval

                            ret=AstOperation(AstOperationKind.ADD_ASSIGN,ret,rhv)

                        case "-=":
                            if ret is None: break

                            t+=1

                            t_afterval,rhv=self.parse_value(t)
                            if rhv is None:
                                fatal(f"invalid value on rhs of assignment at {t[0]}")

                            t=t_afterval

                            ret=AstOperation(AstOperationKind.SUB_ASSIGN,ret,rhv)

                        case "/=":
                            if ret is None: break

                            t+=1

                            t_afterval,rhv=self.parse_value(t)
                            if rhv is None:
                                fatal(f"invalid value on rhs of assignment at {t[0]}")

                            t=t_afterval

                            ret=AstOperation(AstOperationKind.DIV_ASSIGN,ret,rhv)

                        case "*=":
                            if ret is None: break

                            t+=1

                            t_afterval,rhv=self.parse_value(t)
                            if rhv is None:
                                fatal(f"invalid value on rhs of assignment at {t[0]}")

                            t=t_afterval

                            ret=AstOperation(AstOperationKind.MULT_ASSIGN,ret,rhv)

                        #MOD_ASSIGN="%="
                        #XOR_ASSIGN="^="

                        case "|=":
                            if ret is None: break

                            t+=1

                            t_afterval,rhv=self.parse_value(t)
                            if rhv is None:
                                fatal(f"invalid value on rhs of assignment at {t[0]}")

                            t=t_afterval

                            ret=AstOperation(AstOperationKind.BITWISE_OR_ASSIGN,ret,rhv)

                        case "&=":
                            if ret is None: break

                            t+=1

                            t_afterval,rhv=self.parse_value(t)
                            if rhv is None:
                                fatal(f"invalid value on rhs of assignment at {t[0]}")

                            t=t_afterval

                            ret=AstOperation(AstOperationKind.BITWISE_AND_ASSIGN,ret,rhv)

                        case "{":
                            if target_type is None:
                                break

                            t+=1

                            fields:list[tuple[AstValueField|None,AstValue]]=[]

                            while t[0].s!="}":
                                field_target=None
                                if t[0].s==".":
                                    t+=1
                                    field_target=target_type.get_field_by_name(t[0].s)
                                    assert field_target is not None, f"field {t[0]} not in type {target_type.name}"
                                    t+=1
                                    assert t[0].s=="=", f"got instead {t[0]}"
                                    t+=1

                                t_pastvalue,value=self.parse_value(t)
                                if value is None:
                                    fatal(f"expected value, got instead {t[0]}")

                                t=t_pastvalue

                                fields.append((field_target,value))

                                if t[0].s==",":
                                    t+=1
                                    continue

                                break

                            assert t[0].s=="}", f"got instead {t[0]}"
                            t+=1

                            ret=AstCompoundLiteral(fields)

                        case "(":
                            t+=1

                            # cases:
                            # 1) ret is not none, then this is a call operator
                            # 2) ret is none, and a symbol can be parsed from the token iterator that just names a type without a symbol name -> cast operator
                            # 3) else: this is a nesting operator to override operator precedence

                            if ret is None:
                                ts,symbol_def=self.parse_symbol_definition(t,allow_multiple=False,allow_init=False)
                                if symbol_def is not None:
                                    # case 2

                                    t=ts

                                    assert symbol_def.num_sym==1
                                    sym,_=symbol_def.symbols[0]

                                    assert sym.name is None, f"cast to symbol declaration is invalid at {t[0]}"
                                    
                                    assert t[0].s==")", f"got instead {t[0]}"
                                    t+=1

                                    t,cast_value=self.parse_value(t,target_type=sym.ctype)
                                    assert cast_value is not None

                                    ret=AstOperationCast(cast_to_type=sym.ctype,value=cast_value)
                                    continue

                                # case 3
                                t_after_value,nested_value=self.parse_value(t)
                                if nested_value is None:
                                    fatal(f"invalid nested value at {t[0]}")

                                t=t_after_value

                                assert t[0].s==")", f"got instead {t[0]}"
                                t+=1

                                ret=nested_value

                                continue

                            # case 1

                            # TODO check that ret is callable
                            func_type=ret.get_ctype().flatten()
                            assert isinstance(func_type,CTypeFunction), func_type

                            argument_values:list[AstValue]=[]
                            arg_index=-1
                            while t[0].s!=")":
                                arg_index+=1

                                if not (arg_index < len(func_type.arguments) or func_type.has_vararg):
                                    func_type.print(0)
                                    fatal("too many arguments to func")

                                type_as_arg=None
                                if arg_index<len(func_type.arguments):
                                    func_arg=func_type.arguments[arg_index]

                                    # if argument type is __type primitive, parse a type
                                    if CTypePrimitive("__type").can_be_assigned_to(func_arg.ctype):
                                        ts,symdef=self.parse_symbol_definition(t,allow_multiple=False,allow_init=False)
                                        if symdef is None:
                                            # only when argument type is type (i.e. NOT ty_any), is this an error
                                            if func_arg.ctype.can_be_assigned_to(CTypePrimitive.Type()):
                                                fatal(f"expected symbol but found none at {t[0]}")
                                        else:
                                            sym,_=symdef.symbols[0]

                                            type_as_arg=AstValueType(sym.ctype)
                                            t=ts
                                
                                if type_as_arg is not None:
                                    arg_value=type_as_arg
                                else:
                                    t,arg_value=self.parse_value(t)
                                    if arg_value is None:
                                        break

                                argument_values.append(arg_value)

                                if t[0].s==",":
                                    t+=1
                                    continue

                                break

                            assert t[0].s==")", f"got instead {t[0]}"
                            t+=1

                            if isinstance(func_type,CTypeConstFn):
                                if isinstance(func_type, ConstFnSizeof):
                                    ret=func_type.eval(self,argument_values)
                                    continue
                                else:
                                    fatal(f"TODO implement calling const fn {type(func_type)} at {t[0]}")

                            ret=AstFunctionCall(ret,argument_values)

                        case ";":
                            break

                        case _:
                            break

                case other:
                    fatal(f"unimplemented : {other}")

        return t,ret

    def parse_function_definition(self,func_type:CTypeFunction,t:Iter[Token])->tuple[Iter[Token],"AstFunction|None"]:
        if t[0].s!="{":
            return t,None

        t+=1

        func_block=AstFunction(func_type,parent=self)
        t=func_block.parse(t)

        assert t[0].s=="}", f"got instead {t[0]}"
        t+=1

        return t,func_block

    def parse_symbol_definition(self,t:Iter[Token],allow_multiple:bool=True,allow_init:bool=True)->tuple[Iter[Token],SymbolDef|None]:
        "try parsing a symbol definition from t, returns iterator past the content if the return value[1] is not None"

        t_in=t.copy()

        base_ctype:CType|None=None

        ret:list[tuple[Symbol,AstValue|None]]=[]

        while 1:
            # parse new symbol
            symbol:Symbol|None=None
            sym_init_val:AstValue|None=None
            ctype:CType=CType()

            if base_ctype is not None:
                ctype.base_type=base_ctype

            # keep track of open paranthesis
            nesting_depth=0

            while not t.empty:
                match t[0].s:
                    case "extern":
                        ctype.is_extern=True
                        t+=1
                        continue

                    case "_Noreturn":
                        ctype.is_noreturn=True
                        t+=1
                        continue

                    case "thread_local":
                        ctype.is_threadlocal=True
                        t+=1
                        continue

                    case "const":
                        ctype.is_const=True
                        t+=1
                        continue

                    case "static":
                        ctype.is_static=True
                        t+=1
                        continue

                    case "signed":
                        ctype.is_signed=True
                        t+=1
                        continue

                    case "unsigned":
                        ctype.is_signed=False
                        t+=1
                        continue

                    case "long":
                        if ctype.length_mod is None:
                            ctype.length_mod=1
                        else:
                            ctype.length_mod+=1
                        t+=1
                        continue

                    case "short":
                        if ctype.length_mod is None:
                            ctype.length_mod=-1
                        else:
                            ctype.length_mod-=1
                        t+=1
                        continue

                    case "*":
                        if nesting_depth>0:
                            if symbol is None:
                                symbol=Symbol(ctype)

                            symbol.wrap_ctype(lambda c:CTypePointer(c))
                            t+=1
                            continue

                        # cannot point to invalid type
                        if ctype.is_empty_default():
                            break

                        if base_ctype is None:
                            base_ctype=ctype.copy()

                        if symbol is None:
                            symbol=Symbol(ctype)

                        symbol.wrap_ctype(lambda c:CTypePointer(c))

                        t+=1

                        continue

                    case "[":
                        t+=1

                        length_is_static=t[0].s=="static"
                        if length_is_static:
                            t+=1

                        t,array_len=self.parse_value(t)

                        # anonymous arrays are allowed in certain contexts
                        if symbol is None:
                            symbol=Symbol(ctype)

                        if base_ctype is None:
                            base_ctype=ctype.copy()

                        symbol.wrap_ctype(lambda c:CTypeArray(c,array_len,length_is_static=length_is_static))

                        assert t[0].s=="]"
                        t+=1

                        continue

                    case "struct":
                        t+=1

                        struct_name:Token|None=None
                        if t[0].token_type==TokenType.SYMBOL:
                            struct_name=t[0]
                            t+=1

                        struct_base=CTypeStruct(struct_name,None)

                        ctype.base_type=struct_base

                        if t[0].s=="{":
                            t+=1

                            struct_fields:list[CTypeField]=[]

                            while 1:
                                t,field_symdef=self.parse_symbol_definition(t,allow_init=False)
                                if field_symdef is None:
                                    break

                                assert t[0].s==";", f"got instead {t[0]}"
                                t+=1

                                for sym,_ in field_symdef.symbols:
                                    struct_fields.append(CTypeField(sym.name,sym.ctype,parent_ctype=struct_base))

                            struct_base.fields=struct_fields

                            assert t[0].s=="}", f"got instead {t[0]}"
                            t+=1

                        else:
                            # type not defined inline, then must be named
                            assert struct_name is not None

                            # check if inline incomplete type references complete type
                            complete_type=self.getTypeByName(struct_name.s,e_struct=True)
                            if complete_type is not None:
                                ctype.base_type=complete_type

                        continue

                    case "union":
                        t+=1

                        union_name:Token|None=None
                        if t[0].token_type==TokenType.SYMBOL:
                            union_name=t[0]
                            t+=1

                        union_base=CTypeUnion(union_name,None)

                        ctype.base_type=union_base

                        if t[0].s=="{":
                            t+=1
                            
                            union_fields:list[CTypeField]=[]

                            while 1:
                                t,field_symdef=self.parse_symbol_definition(t)
                                if field_symdef is None:
                                    break

                                assert t[0].s==";", f"got instead {t[0]}"
                                t+=1

                                for sym,_ in field_symdef.symbols:
                                    union_fields.append(CTypeField(sym.name,sym.ctype,parent_ctype=union_base))

                            union_base.fields=union_fields

                            assert t[0].s=="}", f"got instead {t[0]}"
                            t+=1
                        else:
                            # type not defined inline, then must be named
                            assert union_name is not None

                            # check if inline incomplete type references complete type
                            complete_type=self.getTypeByName(union_name.s,e_union=True)
                            if complete_type is not None:
                                ctype.base_type=complete_type

                        continue

                    case "enum":
                        t+=1

                        enum_name:Token|None=None
                        fields:list[tuple[Symbol,AstValue]]|None=None

                        ctype_int=self.getTypeByName("int")
                        assert ctype_int is not None

                        if t[0].token_type==TokenType.SYMBOL:
                            enum_name=t[0]
                            t+=1

                        if t[0].s=="{":
                            t+=1

                            fields=[]

                            last_field_value=AstValueNumericLiteral(0,ctype_int)
                            while not t.empty and t[0].s!="}":
                                assert t[0].token_type==TokenType.SYMBOL, f"expected symbol, got instead {t[0]}"
                                field_name=t[0]

                                t+=1

                                field_value=None
                                if t[0].s=="=":
                                    t+=1

                                    t_after_val,field_value=self.parse_value(t)
                                    if field_value is None:
                                        fatal(f"got instead {t[0]}")

                                    t=t_after_val

                                if field_value is None:
                                    field_value=AstOperation(AstOperationKind.ADD,last_field_value,AstValueNumericLiteral(1,ctype_int))

                                last_field_value=field_value

                                fields.append((Symbol(ctype_int,field_name),field_value))

                                if t[0].s==",":
                                    t+=1
                                    continue

                                break

                            assert t[0].s=="}"
                            t+=1

                        enum_base=CTypeEnum(ctype_int,enum_name,fields)
                        ctype.base_type=enum_base

                        continue

                    case "(":
                        t+=1
                        # function decl must have a symbol, but a function type may be anonymous

                        found_func_decl=True
                        t_before_func_decl=t.copy()

                        arguments:list[Symbol]=[]
                        has_vararg=False

                        while t[0].s!=")":
                            if t[0].s==VARARG_ARGNAME:
                                has_vararg=True
                                t+=1
                                break

                            t_sym_def,symbol_def=self.parse_symbol_definition(t,allow_multiple=False,allow_init=False)
                            if symbol_def is None:
                                break

                            if symbol_def.num_sym==0:
                                break

                            t=t_sym_def

                            sym,_=symbol_def.symbols[0]

                            arguments.append(sym)
                            if t[0].s==",":
                                t+=1
                                continue

                            break

                        if t[0].s!=")":
                            t=t_before_func_decl
                            found_func_decl=False

                        if found_func_decl:
                            assert t[0].s==")", f"got instead {t[0]}"
                            t+=1

                            ctype.nest()
                            assert ctype.base_type is not None

                            ctype.base_type=CTypeFunction(ctype.base_type,arguments,has_vararg=has_vararg)

                            if symbol is None:
                                symbol=Symbol(ctype)

                            continue

                        # nested type declaration, allows for some precedence overrides, e.g. applying modifiers to the symbol, rather than the base type
                        nesting_depth+=1
                        if symbol is None:
                            symbol=Symbol(ctype)

                        continue

                    case other:
                        if other==")" and nesting_depth>0:
                            t+=1
                            nesting_depth-=1
                            continue

                        # check for existing type by name
                        other_as_type=self.getTypeByName(other)
                        if other_as_type is not None:
                            ctype.base_type=other_as_type
                            t+=1
                            continue

                        elif other=="=" and allow_init and symbol is not None:
                            t+=1
                            t,sym_init_val=self.parse_value(t,target_type=symbol.ctype)
                            assert sym_init_val is not None, f"no value for symbol init at {t[0]}"
                            continue

                        else:
                            if not t[0].is_valid_symbol():
                                break

                            if ctype.is_empty_default():
                                break

                            if base_ctype is None:
                                base_ctype=ctype.copy()

                            symbol=Symbol(ctype=ctype,name=t[0])

                            t+=1
                            continue 

                break

            if ctype.is_empty_default():
                break

            if nesting_depth>0:
                fatal("unclosed paranthesis")

            # create anonymous symbol from ctype is type exists but name does not
            if symbol is None:
                symbol=Symbol(ctype,name=None)

            # catch invalid type definitions, e.g. pointer to nothing
            sym_val_str=symbol.ctype.validate()
            if sym_val_str is not None:
                break

            # flatten type of symbol
            symbol.ctype=symbol.ctype.flatten()

            # append symbol to list of defined symbols for later return
            ret.append((symbol,sym_init_val))

            # check for multiple definitions, e.g. "int a,b;"
            if t[0].s=="," and allow_multiple:
                t+=1

                assert base_ctype is not None
                #fatal("TODO")
                continue

            # stop parsing otherwise
            break

        if len(ret)==0:
            return t_in,None

        return t,SymbolDef(ret)

class AstForLoop(Block,Statement):
    """
    for loop

    hacky: always has two statements.
    - first statement is init statement (is in self.statment list because ingest into that list adds symbols defined there to loop scope)
    - second statement is the loop body
    """
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
        print(ind(indent)+"for:")
        print(ind(indent+1)+"init:")
        self.init.print(indent+2)
        print(ind(indent+1)+"cond:")
        if self.condition is not None:
            self.condition.print(indent+2)
        print(ind(indent+1)+"step:")
        if self.step is not None:
            self.step.print(indent+2)

        print(ind(indent+1)+"body:")
        assert len(self.statements)==2, f"{len(self.statements)}"
        self.statements[1].print(indent+2)

class AstWhileLoop(Block,Statement):
    def __init__(self,cond:AstValue,statement:Statement,do_while:bool=False):
        super().__init__()
        self.cond=cond
        self.statement=statement
        self.do_while=do_while

    @tp.override
    def print(self,indent:int):
        print(ind(indent)+"while:")
        print(ind(indent+1)+"cond:")
        self.cond.print(indent+2)
        print(ind(indent+1)+"body:")
        self.statement.print(indent+2)

class AstLoopBreak(Statement):
    def __init__(self):
        super().__init__()
    @tp.override
    def print(self,indent:int):
        print(ind(indent)+"break")

class AstLoopContinue(Statement):
    def __init__(self):
        super().__init__()
    @tp.override
    def print(self,indent:int):
        print(ind(indent)+"continue")

class AstEmptyStatement(Statement):
    def __init__(self):
        super().__init__()
    @tp.override
    def print(self,indent:int):
        print(ind(indent)+"stmt: ;")

class AstGoto(Statement):
    def __init__(self,label:Token):
        super().__init__()
        self.label=label

    @tp.override
    def print(self,indent:int):
        print(ind(indent)+f"goto: {self.label.s}")

class AstLabel(Statement):
    def __init__(self,label:Token):
        super().__init__()
        self.label=label

    @tp.override
    def print(self,indent:int):
        print(ind(indent)+f"label: {self.label.s}")

class AstSwitch(Block,Statement):
    def __init__(self,val:AstValue,body:Statement):
        super().__init__()
        self.val=val
        self.body=body

    @tp.override
    def print(self,indent:int):
        print(ind(indent)+"switch:")
        print(ind(indent+1)+"value:")
        self.val.print(indent+2)
        print(ind(indent+1)+"body:")
        self.body.print(indent+2)

class AstSwitchDefault(Statement):
    def __init__(self):
        super().__init__()
    @tp.override
    def print(self,indent:int):
        print(ind(indent)+"switchcase default")

class AstSwitchCase(Statement):
    def __init__(self,val:AstValue):
        super().__init__()
        self.val=val

    @tp.override
    def print(self,indent:int):
        print(ind(indent)+"case:")
        self.val.print(indent+1)

class AstIf(Block,Statement):
    def __init__(self,if_condition:AstValue,if_statement:Statement,else_statement:Statement|None):
        super().__init__()

        self.if_condition=if_condition
        self.if_statement=if_statement
        self.else_statement=else_statement

    @tp.override
    def print(self,indent:int):
        print(ind(indent)+"if:")
        print(ind(indent+1)+"condition:")
        self.if_condition.print(indent+2)
        print(ind(indent+1)+"then:")
        self.if_statement.print(indent+2)
        if self.else_statement is not None:
            print(ind(indent+1)+"else:")
            self.else_statement.print(indent+2)

class AstBlock(Block,Statement):
    def __init__(self,
        parent:Block|None=None,
    ):
        super().__init__(parent=parent)

    @tp.override
    def print(self,indent:int):
        print(ind(indent)+"block:")
        for statement in self.statements:
            statement.print(indent+1)

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

class ConstFnSizeof(CTypeConstFn):
    "sizeof compile time function"
    def __init__(self):
        super().__init__(return_kind=AstValue,arguments=[("t",CTypePrimitive("__ty_any"))])

    @tp.override
    def eval(self,block:Block,arguments:list["AstValue"])->"AstValue":
        assert len(arguments)==len(self.arguments)

        # TODO actually calculate this based on the type
        return AstValueNumericLiteral(4,block.getTypeByName("int") or fatal(""))

class Ast:
    "represents one "
    def __init__(self,tokens:list[Token]):
        self.t=Iter(tokens)

        self.block:Block=Block(
            types={
                "void":CTypePrimitive.Void(),
                "int":CTypePrimitive.Int(),
                "char":CTypePrimitive.Char(),

                "float":CTypePrimitive.Float(),
                "double":CTypePrimitive.Double(),
                "bool":CTypePrimitive.Bool(),

                "__builtin_va_list":CTypePrimitive("__builtin_va_list"),
            },
            symbols={
                "sizeof":Symbol(ConstFnSizeof()),

                "nullptr":Symbol(
                    CTypePointer(
                        CTypePrimitive.Void()
                    ),
                    Token(
                        "nullptr",src_loc=SourceLocation.placeholder(),token_type=TokenType.SYMBOL
                    )
                ),

                "__builtin_va_start": Symbol(CTypeFunction(
                    CTypePrimitive("void"),[
                        Symbol(CTypePrimitive("__builtin_va_list")),
                        Symbol(CTypePrimitive.Any())
                    ]),
                    Token(
                        "__builtin_va_start",src_loc=SourceLocation.placeholder(),token_type=TokenType.SYMBOL
                    )), #__builtin_va_start(v,l)
                "__builtin_va_end": Symbol(CTypeFunction(
                    CTypePrimitive("void"),[
                        Symbol(CTypePrimitive("__builtin_va_list")),
                    ]),
                    Token(
                        "__builtin_va_end",src_loc=SourceLocation.placeholder(),token_type=TokenType.SYMBOL
                    )), #__builtin_va_end(v)
                "__builtin_va_arg": Symbol(CTypeFunction(
                    CTypePrimitive("void"),[
                        Symbol(CTypePrimitive("__builtin_va_list")),
                        Symbol(CTypePrimitive("__type"))
                    ]),
                    Token(
                        "__builtin_va_arg",src_loc=SourceLocation.placeholder(),token_type=TokenType.SYMBOL
                    )), #__builtin_va_arg(v,l)
                "__builtin_va_copy": Symbol(CTypeFunction(
                    CTypePrimitive("void"),[
                        Symbol(CTypePrimitive("__builtin_va_list")),
                        Symbol(CTypePrimitive("__builtin_va_list")),
                    ]),
                    Token(
                        "__builtin_va_copy",src_loc=SourceLocation.placeholder(),token_type=TokenType.SYMBOL
                    )), #__builtin_va_copy(d,s)
            },
        )

        self.t=self.block.parse(self.t)

        if not self.t.empty:
            fatal(f"leftover tokens at end of file: {str(self.t[0])} ...")
