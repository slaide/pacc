import concurrent.futures as fut
import subprocess
import typing as tp
import time
import os
import platform
import shlex
from enum import Enum
from pathlib import Path
import json
import dataclasses
from dataclasses import dataclass

from tqdm import tqdm



def get_num_cores()->int:
    """
    get number of logical cores on the host system, minus 1

    returns at least 1
    """

    max_num_cores=1
    try:
        native_num_cores=os.cpu_count()
        if native_num_cores is not None:
            max_num_cores=native_num_cores
        elif platform.system()=="Darwin":
            max_num_cores=int(os.popen("sysctl -n hw.logicalcpu").read())
        elif platform.system()=="Linux":
            max_num_cores=int(os.popen("nproc").read())
    except:
        pass

    if max_num_cores==1:
        return 1
    
    # leave 1 core for the system
    return max_num_cores - 1

class ArgStore(str,Enum):
    presence_flag="presence_flag"
    store_value="store_value"

class Arg:
    def __init__(self,
        name:str,
        short:tp.Optional[str]=None,
        help:str="",
        default:tp.Optional[tp.Any]=None,
        key:tp.Optional[str]=None,
        type:tp.Type=str,
        arg_store_op:ArgStore=ArgStore.store_value,
        options:tp.Optional[tp.Union[tp.List[tp.Any],tp.Dict[str,tp.Any]]]=None
    ):
        self.name=name
        self.short=short
        self.help=help
        self.default=default
        self.key=key or self.name.lstrip("-").replace("-","_")
        self.type=type
        self.arg_store_op=arg_store_op
        if self.arg_store_op==ArgStore.presence_flag and self.default is None:
            self.default=False
        self.options=options

class ArgParser:
    def __init__(self,program_info:str):
        self.program_info=program_info
        self.args:tp.List[Arg]=[]

    def print_help(self):
        print(self.program_info)
        print()
        print("Arguments:")

        arg_strs=[]
        for arg in self.args:
            short_arg_name=(arg.short+' ') if arg.short else ''
            arg_strs.append((f"  {short_arg_name}{arg.name}",f" : {arg.help}"))

        longest_prefix=max(len(a[0]) for a in arg_strs)
        for (arg_pre,arg_post),arg in zip(arg_strs,self.args):
            print(arg_pre,arg_post,sep=" "*(longest_prefix-len(arg_pre)))
            match arg.arg_store_op:
                case ArgStore.presence_flag:
                    pass
                case ArgStore.store_value:
                    print(" "*(longest_prefix+4),f"- default: {arg.default}",sep=None)

            if arg.options is not None:
                if isinstance(arg.options,dict):
                    options=[f"{k}={v}" for k,v in arg.options.items()]
                else:
                    options=[str(o) for o in arg.options]
                print(" "*(longest_prefix+4),f"- options: {', '.join(options)}",sep=None)

    def add(self,*args,**kwargs):
        self.args.append(Arg(*args,**kwargs))

    def parse(self,args:tp.List[str])->dict:
        valid_args=dict()
        for arg in self.args:
            if arg.name in valid_args:
                raise ValueError(f"Duplicate argument name {arg.name}")
            valid_args[arg.name]=arg
            if arg.short is not None:
                if arg.short in valid_args:
                    raise ValueError(f"Duplicate argument short {arg.short}")
                valid_args[arg.short]=arg

        arg_values={
            a.key:a.default for a in self.args
        }

        for a in args:
            arg_split=a.split("=",2)
            arg_name=arg_split[0]
            arg_value=arg_split[1] if len(arg_split)==2 else None

            arg=valid_args.get(arg_name,None)
            if arg is not None:
                value=None
                match arg.arg_store_op:
                    case ArgStore.presence_flag:
                        value=True
                    case ArgStore.store_value:
                        value=arg.type(arg_value) if arg_value is not None else arg.default
                    case _other:
                        raise ValueError(f"Unknown arg store operation {_other}")

                if arg.options is not None:
                    if value not in arg.options:
                        raise ValueError(f"Invalid value '{value}' for argument {arg_name}, valid values are {arg.options}")

                arg_values[arg.key]=value
            else:
                raise ValueError(f"Unknown arg {a}")

        return arg_values


@dataclass
class SingleFileCache:
    file:str
    time:float

    def to_dict(self)->dict:
        return dataclasses.asdict(self)

@dataclass
class CommandCache:
    cmd:str
    time:float
    in_files:tp.List[SingleFileCache]
    out_files:tp.List[SingleFileCache]

    def to_dict(self)->dict:
        return dataclasses.asdict(self)
    
    @staticmethod
    def from_dict(d:dict)->"CommandCache":
        return CommandCache(
            cmd=d["cmd"],
            time=d["time"],
            in_files=[SingleFileCache(**i) for i in d["in_files"]],
            out_files=[SingleFileCache(**i) for i in d["out_files"]]
        )

class CacheManager:
    def __init__(self,rebuild:bool=False):
        self.rebuild=rebuild
        self.cache_file_name=".build_cache.json"
        self.build_cache:tp.Dict[str,CommandCache]=dict()

        self.new_keys=set()

        if self.rebuild:
            self.build_cache=dict() 
        elif Path(self.cache_file_name).exists():
            with open(self.cache_file_name,"r") as f:
                self.build_cache={k:CommandCache.from_dict(v) for k,v in json.load(f).items()}

    def flush(self):
        " write results to disk "

        # remove keys that are no longer in use
        for k in list(self.build_cache.keys()):
            if not k in self.new_keys:
                del self.build_cache[k]

        with open(self.cache_file_name,"w") as f:
            json.dump({k:v.to_dict() for k,v in self.build_cache.items()},f)

    def cmd_finished(self,c:"Command"):
        """ can be called on finished commands to cache them """
        current_time=time.time()

        new_cmd_cache=CommandCache(
            cmd=c.cmd,
            time=current_time,
            in_files=[SingleFileCache(file=f,time=Path(f).stat().st_mtime) for f in c.in_files],
            out_files=[SingleFileCache(file=f,time=Path(f).stat().st_mtime) for f in c.out_files]
        )

        self.new_keys.add(c.cmd)
        self.build_cache[c.cmd]=new_cmd_cache

    def cmd_is_cached(self,c:"Command")->bool:
        """ check if a command is cached """

        # get time of last command run
        if not c.cmd in self.build_cache:
            return False
        
        cmd_cache=self.build_cache[c.cmd]

        old_in_files={f.file:f for f in cmd_cache.in_files}
        old_out_files={f.file:f for f in cmd_cache.out_files}
        new_in_files=c.in_files
        
        for f in new_in_files:
            # not in cache
            if not f in old_in_files:
                return False

            # file does not exist (e.g. requires building from dependent command)
            if not Path(f).exists():
                return False
            
            f_cache=old_in_files[f]

            # if file has changed since last build
            if Path(f).stat().st_mtime>f_cache.time:
                return False

        for f in c.out_files:
            # not in cache
            if not f in old_out_files:
                return False

            # file does not exist (needs to be recreated)
            if not Path(f).exists():
                return False

            f_cache=old_out_files[f]

            # if file has changed since last build
            if Path(f).stat().st_mtime>f_cache.time:
                return False

        return True

class Command:
    pool:tp.Optional[fut.ThreadPoolExecutor]=None
    show_cmds:bool=False
    cmd_cache:tp.Optional[CacheManager]=None

    @staticmethod
    def build(cmd:"Command"):
        all_cmds:tp.Set["Command"]=set()
        def get_all_cmds(c:"Command"):
            all_cmds.add(c)
            for f in c.depends_on:
                get_all_cmds(f)
        
        get_all_cmds(cmd)

        if len(all_cmds)==0:
            return
        
        tqdm_iter=tqdm(total=len(all_cmds),desc="Building",unit="cmd")

        while 1:
            # go through all cmds, if one is ready, process it
            # if none are ready, sleep for a bit
            # if all are done, break
            finished=True
            remove_set=set()
            for c in all_cmds:
                if c.ready and not c.done:
                    if not c.running:
                        was_cached=False
                        if not c.phony:
                            if Command.cmd_cache is not None:
                                was_cached=Command.cmd_cache.cmd_is_cached(c)

                        if not was_cached:
                            c.process()
                        else:
                            c.was_cached=True

                    finished=False
                
                if c.done:
                    if not c.was_cached:
                        if Command.show_cmds:
                            print(f"{c.cmd}")

                        res=c.res
                        assert res is not None
                        (res_e,res_str)=res
                        if res_e!=0:
                            print(f"Error in command {c.cmd}")
                            print(res_str)
                            exit(1)

                    if not c.phony:
                        if Command.cmd_cache is not None:
                            Command.cmd_cache.cmd_finished(c)

                    tqdm_iter.update(1)

                    remove_set.add(c)

                if not c.ready:
                    finished=False
            
            for r in remove_set:
                all_cmds.remove(r)

            if finished:
                break

            time.sleep(1e-3)


    def __init__(self,cmd:str,in_files:tp.List[str]=[],out_files:tp.List[str]=[],phony:bool=False,shell:bool=False):
        self.in_files=in_files
        self.out_files=out_files

        self.cmd=cmd
        """ command to run """
        self.phony=phony
        """ if true, the command is not cached """

        self.shell=shell

        self.depends_on:tp.Set[Command]=set()
        self.followed_by:tp.Set[Command]=set()

        self._future:tp.Optional[fut.Future[tp.Tuple[int,str]]]=None
        self.was_cached=False

    def depends(self,*other_cmds:"Command")->"Command":
        for other_cmd in other_cmds:
            other_cmd.followed_by.add(self)
            self.depends_on.add(other_cmd)
        return self

    @property
    def res(self)->tp.Optional[tp.Tuple[int,str]]:
        if not self.done:
            return None
        
        if self.was_cached:
            return (0,"")

        assert self._future is not None
        return self._future.result()
    
    @property
    def done(self)->bool:
        " Returns true if the command has been executed "

        if self.was_cached:
            return True
        
        if self._future is None:
            return False

        return self._future.done()

    @property
    def ready(self)->bool:
        " Returns true if all dependencies are done "
        ret=all(f.done for f in self.depends_on)
        return ret
    
    @property
    def running(self)->bool:
        " returns True if the task is running (rather, submitted to the pool for execution, may not be actively executing currently) "
        if self._future is None:
            return False

        return True

    def process(self):
        " submit the command to the pool for processing "
        assert self.ready

        def run()->tp.Tuple[int,str]:
            split_args=shlex.split(self.cmd)
            if len(split_args)==0:
                return (0,"")
            
            if self.shell:
                p=subprocess.run(self.cmd,stdout=subprocess.PIPE,stderr=subprocess.PIPE,shell=True)
            else:
                p=subprocess.run(split_args,stdout=subprocess.PIPE,stderr=subprocess.PIPE)

            ret=""
            if p.returncode!=0:
                ret+=f"running $ {self.cmd}\n"
                ret+=p.stdout.decode()
                ret+=p.stderr.decode()

            return (p.returncode,ret)
            
        if Command.pool is None:
            # write custom future which is executed here, just preserving the interface to the outside
            s_fut=fut.Future()
            s_fut.set_result(run())
            self._future=s_fut
        else:
            self._future=Command.pool.submit(run)
