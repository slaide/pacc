import sys, io, time
from dataclasses import dataclass, field
import typing as tp
from enum import Enum
from pathlib import Path
import inspect
import re

from libbuild import RED,GREEN,RESET,LIGHT_GRAY,ORANGE

def ind(n:int)->str:
    # U+FF5C ｜
    # U+007C |
    return LIGHT_GRAY+("｜"*n)+RESET

def fatal(message:str="",exit_code:int=-1)->tp.NoReturn:
    current_frame=inspect.currentframe()
    assert current_frame is not None

    frames=[current_frame]

    while (current_frame:=current_frame.f_back) is not None:
        frames.append(current_frame)

    # omit bottom of stack (which is this function) and top of stack (which is the module)
    # and reverse to print lowest stack information last
    frames=reversed(frames[1:-2])

    print()

    _=sys.stdout.write("FATAL >>>\n")

    for current_frame in frames:
        lineno=current_frame.f_lineno
        filename=current_frame.f_code.co_filename

        func_name=".".join(current_frame.f_code.co_qualname.split(".")[2:])
        func_name=current_frame.f_code.co_qualname

        _=sys.stdout.write(f" {LIGHT_GRAY}{filename}:{lineno} -{RESET} {func_name}\n")

    if len(message)>0:
        _=sys.stdout.write(f" >>> {RED}{message}{RESET}\n")
    else:
        _=sys.stdout.write(f" >>> \n")

    _=sys.stdout.flush()

    sys.exit(exit_code)

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

    def __getitem__(self,i:int)->"T":
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