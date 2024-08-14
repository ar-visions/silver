from dataclasses import dataclass, field, fields, is_dataclass
from typing import Union, Type, List, OrderedDict, Tuple, Any
from pathlib import Path
import numpy as np
import os, sys, subprocess, platform
from clang.cindex import Index, CursorKind, TypeKind

build_root = ''
is_debug = False

no_arg_methods = [
    'init', 'dealloc'
]

debug = 1

operators = {
    '+':   'add',
    '-':   'sub',
    '*':   'mul',
    '/':   'div',
    '||':  'or',
    '&&':  'and',
    '^':   'xor',
    '>>':  'right',
    '<<':  'left',
    ':':   'assign',
    '+=':  'assign_add',
    '-=':  'assign_sub',
    '*=':  'assign_mul',
    '/=':  'assign_div',
    '|=':  'assign_or',
    '&=':  'assign_and',
    '^=':  'assign_xor',
    '>>=': 'assign_right',
    '<<=': 'assign_left',
    '%=':  'mod_assign'
}

keywords = [ 'class',  'proto',    'struct', 'import',
             'init',   'destruct', 'ref',    'const',  'volatile',
             'return', 'asm',      'if',     'switch',
             'while',  'for',      'do',     'signed', 'unsigned' ]

assign   = [ ':',  '+=', '-=',  '*=',  '/=', '|=',
             '&=', '^=', '>>=', '<<=', '%=' ]

# EIdent are defined as ones that have combined these keywords into a decoration map
consumables     = ['ref', 'struct', 'class', 'enum', 'const', 'volatile', 'signed', 'unsigned', 'dim', 'import'] # countof is used for i32[4] == 4

# global to track next available ID (used in ENode matching)
_next_id        = 0

# default paths for Clang and gcc
_default_paths  = ['/usr/include/', '/usr/local/include']

def make_hashable(item):
    if isinstance(item, list):
        return tuple(make_hashable(sub_item) for sub_item in item)
    elif isinstance(item, dict):
        return tuple(sorted((make_hashable(k), make_hashable(v)) for k, v in item.items()))
    elif isinstance(item, set):
        return frozenset(make_hashable(sub_item) for sub_item in item)
    return item

class ENode:
    def __post_init__(self):
        global _next_id
        self.id = _next_id
        _next_id += 1
    
    def __repr__(self):
        attrs = ', '.join(f'{key}={value!r}' for key, value in vars(self).items())
        return f'{type(self).__name__}({attrs})'
    
    def to_serialized_string(self):
        attrs = ', '.join(f'{key}={self.serialize_value(value)}' for key, value in vars(self).items())
        return f'{type(self).__name__}({attrs})'

    @staticmethod
    def serialize_value(value):
        if isinstance(value, str):
            return f'"{value}"'
        elif isinstance(value, ENode):
            return value.to_serialized_string()
        elif isinstance(value, list):
            return '[' + ', '.join(ENode.serialize_value(v) for v in value) + ']'
        return repr(value)

    def __hash__(self):
        return hash(self.id)

    def __eq__(self, other):
        if not isinstance(other, ENode):
            return False
        return self.id == other.id

def is_assign(t):
    i = index_of(assign, str(t))
    if (i == -1): return None
    return assign[i]

def is_alpha(s):
    s = str(s)
    if index_of(keywords, s) >= 0:
        return False
    if len(s) > 0:  # Ensure the string is not empty
        return s[0].isalpha() or s[0] == '_'
    return False  # Return False for an empty string

def index_of(a, e):
    try:
        return a.index(e)
    except ValueError:
        return -1

def rem_spaces(text):
    text = text.replace('* ', '*')
    text = text.replace(' *', '*')
    return text

def _make_hashable(value: Any) -> Any:
    if isinstance(value, list):
        return tuple(map(_make_hashable, value))
    elif isinstance(value, dict):
        return tuple(sorted((k, _make_hashable(v)) for k, v in value.items()))
    elif isinstance(value, set):
        return frozenset(map(_make_hashable, value))
    return value
    
class HeapType:
    placeholder: bool

class StructType:
    placeholder: bool

class AType:
    placeholder: bool

class Void:
    pass

@dataclass
class EModel(ENode):
    name:str
    size:int
    integral:bool
    realistic:bool
    type:Type
    id:int = None

models:OrderedDict[str, EModel] = OrderedDict()
models['atype']       = EModel(name='atype',       size=8, integral=0, realistic=0, type=AType)
models['allocated']   = EModel(name='allocated',   size=8, integral=0, realistic=0, type=HeapType)
models['struct']      = EModel(name='struct',      size=8, integral=0, realistic=0, type=StructType)
models['boolean-32']  = EModel(name='boolean_32',  size=4, integral=1, realistic=0, type=bool)
models['unsigned-8']  = EModel(name='unsigned_8',  size=1, integral=1, realistic=0, type=np.uint8)
models['unsigned-16'] = EModel(name='unsigned_16', size=2, integral=1, realistic=0, type=np.uint16)
models['unsigned-32'] = EModel(name='unsigned_32', size=4, integral=1, realistic=0, type=np.uint32)
models['unsigned-64'] = EModel(name='unsigned_64', size=8, integral=1, realistic=0, type=np.uint64)
models['signed-8']    = EModel(name='signed_8',    size=1, integral=1, realistic=0, type=np.int8)
models['signed-16']   = EModel(name='signed_16',   size=2, integral=1, realistic=0, type=np.int16)
models['signed-32']   = EModel(name='signed_32',   size=4, integral=1, realistic=0, type=np.int32)
models['signed-64']   = EModel(name='signed_64',   size=8, integral=1, realistic=0, type=np.int64)
models['real-32']     = EModel(name='real_32',     size=4, integral=0, realistic=1, type=np.float32)
models['real-64']     = EModel(name='real_64',     size=8, integral=0, realistic=1, type=np.float64)
models['real-128']    = EModel(name='real_128',    size=16, integral=0, realistic=1, type=np.float128)
models['void']        = EModel(name='void',        size=0, integral=0, realistic=1, type=Void)
models['cstr']        = EModel(name='cstr',        size=8, integral=0, realistic=0, type=np.uint64)
models['symbol']      = EModel(name='symbol',      size=8, integral=0, realistic=0, type=np.uint64)
models['handle']      = EModel(name='handle',      size=8, integral=0, realistic=0, type=np.uint64)

test1 = 0

# tokens can be decorated with any data
@dataclass
class EIdent(ENode):
    decorators: OrderedDict      = field(default_factory=OrderedDict)
    list:       List['Token']    = field(default_factory=list)
    ident:      str              = None # all combinations of tokens are going to yield a reduced name that we use for matching.  we remove decorators from this ident, but may not for certain ones.  we can change type matching with that
    initial:    str              = None
    module:    'EModule'         = None
    is_fp:      bool             = False
    kind:       Union[str | TypeKind] = None
    base:      'EMember'         = None
    meta_types: List['EIdent']   = field(default_factory=list)
    args:       OrderedDict      = None
    #
    def __init__(
            self,
            tokens:     Union['EModule', 'EIdent', str, 'EMember', 'Token', List['Token']] = None,
            kind:       Union[str | TypeKind] = None,
            meta_types: List['EIdent']   = None, # we may provide meta in the tokens, in which case it replaces it; or, we can provide meta this way which resolves differently; in object format, it may encounter tokens or strings
            decorators: OrderedDict      = OrderedDict(),
            base:      'EMember'         = None,
            module:    'EModule'         = None):
        self.module = module
        self.meta_types = meta_types if meta_types else list()
        
        assert module, "module must be given to EIdent, as we lookup types here for use in decorating"
        self.decorators = decorators or OrderedDict()
        self.base = base
        if isinstance(tokens, EIdent):
            self.decorators = tokens.decorators.copy() if tokens.decorators else OrderedDict()
            self.list       = tokens.list.copy() if tokens.list else []
        elif isinstance(tokens, EMember):
            if isinstance(tokens, EClass):  self.decorators['class']  = True
            if isinstance(tokens, EStruct): self.decorators['struct'] = True
            if isinstance(tokens, EEnum):   self.decorators['enum']   = True
            if tokens.static: self.decorators['static'] = True
            self.list       = [Token(value=tokens.name, line_num=0)]
        elif isinstance(tokens, Token):
            self.list       = [tokens]  # Initialize with a single Token
        elif isinstance(tokens, list):
            self.list       = tokens
        elif isinstance(tokens, str): # we have decorators inside list until we 'peek' to resolve
            # we parse function pointers here in string form
            # this does mean we need to sometimes read ahead in token processing to form a string, which is not ideal.
            # otherwise, value could be a read-cursor to read identity with
            # that way we have a simple ident = Ident(tokens=stream)
            # implement parse_type_tokens here
            if index_of(tokens, '(unnamed') >= 0:
                if   index_of(tokens, 'struct') >= 0: tokens = 'struct anonymous'
                elif index_of(tokens, 'enum')   >= 0: tokens = 'enum anonymous'
                elif index_of(tokens, 'union')  >= 0: tokens = 'union anonymous'
                else: assert False, 'case not handled'
            self.initial    = tokens
            read_tokens     = module.read_tokens(tokens)
            if index_of(tokens, '*)') >= 0:
                tokens
            ident           = EIdent.parse(module, read_tokens, 0)
            if ident['ref'] == 2:
                ident
            self.decorators = ident.decorators.copy() if ident.decorators else OrderedDict()
            self.list       = ident.list.copy() if ident.list else []
        else:
            self.list = []
        
        if kind == 'pointer' or kind == TypeKind.POINTER:
            assert self['ref'], 'Clang gave TypeKind.POINTER without describing'

        self.updated()
        if self.meta_types:
            assert self.get_def(), 'no definition for type: %s' % self.ident
            for m in self.meta_types:
                assert m.get_def(), 'no definition for meta: %s' % m.ident

    def peek(module):
        module.push_token_state(module.tokens, module.index)
        ident = EIdent.parse(module, module.tokens, offset=module.index)
        module.pop_token_state()
        return ident
    
    def parse(module, tokens:List['Token'] = None, offset=0):
        push_state = False
        if isinstance(tokens, str):
            t = EIdent(module=module)
            for token in tokens.split():
                t += Token(value=token)
            tokens = t
        if tokens:
            push_state = True
            module.push_token_state(tokens, offset)
        result     = EIdent(module=module) # important to not set tokens=module here
        ahead      = offset
        proceed    = True
        ref        = 0
        silver_ref = 0
        f          = module.peek_token()
        if f == 'ref':
            silver_ref = 1
            ref    += 1
            proceed = False
            f       = module.next_token()
        
        heuristics = True
        if str(f) in module.defs:
            mdef = module.defs[f]
            if isinstance(mdef, EMethod):
                module.consume(f)
                result.list.append(f)
                proceed    = False
                heuristics = False
        
        # mem = module.lookup_member(str(f)) -- experimental
        while proceed:
            proceed = False
            global consumables
            offset = 0
            for con in consumables:
                n = module.peek_token(offset)
                if n in ['(', '[', ']']: break
                if n == con:
                    offset += 1
                    result[con] = 1
                    proceed = True
                    if module.eof():
                        if push_state:
                            module.pop_token_state()
                        return result
            module.consume(offset)
        
        while heuristics:
            if module.eof(): break
            if module.peek_token() == ')': break
            if module.peek_token() == '(': break
            t = module.next_token()
            is_extend = t.value in ['long', 'short', 'unsigned', 'signed']
            is_ext = t.value in ['unsigned', 'signed']
            if not is_extend and not is_alpha(t.value):
                #result = EIdent(module=module)
                assert False, 'debug'
                break
            result.list.append(t)
            if module.eof(): break
            p = module.peek_token()
            require_next = False
            if p == '*':
                ref += 1
                module.consume('*')
                p_last = p
                p = module.peek_token()
                while (p == '*'):
                    ref += 1
                    p_last = p
                    p = module.next_token()
                if p != 'const':
                    assert p_last != 'const', 'illegal type'
                    break
            elif p == '::':
                require_next = True
                module.consume('::')
            else:
                break
            if module.eof():
                assert require_next, 'expect type-identifier after ::'
                break
            p = module.next_token()
            is_open = p == '['
            if   is_open and silver_ref:
                break # we want to return ref TheType; the caller will perform heuristics
            elif is_open:
                dim = []
                while (p == '['):
                    p = module.next_token()
                    if p == ']':
                        v = -1
                        break
                    v = p
                    assert module.next_token() == ']', 'expected ] character in C dimensional array declaration'
                    dim.append(int(v.value))
                    assert str(int(v.value) == v.value), 'incorrect numeric format given'
                    if module.eof():
                        break
                    p = module.next_token()
                result['dim'] = dim
            if ref: break
            if not p or (not p.value in ['::'] and not is_ext): break # this keyword requires single token types for now
            if module.eof():
                assert False, 'debug 2'
                result = None
                break

        if ref > 0: # ref string* list  would be string**; we are now allowing *'s in the EIdent identity with ref:True == *
            result['ref'] = ref
        result.updated()
        
        # lets see if this whole thing is a function pointer; EIdent stores as much as this.. nothing more!
        if not module.eof():
            f = module.peek_token(0)
            if f == '(': # simple logic now.. the ONLY use of parenthesis is in function pointer declaration after an ident is read
                # expect * after optional token of alpha
                a = module.peek_token(1)
                b = module.peek_token(2)

                if a == '*':
                    module.consume(3)
                    #ahead += 2
                    assert b == ')', 'expected ) after * in function pointer declaration'
                else:
                    module.consume(4)
                    assert is_alpha(a), 'expected alpha-name of function pointer, or *'
                    assert b == ')', 'expected '
                
                begin_arg = module.peek_token()
                assert begin_arg == '(', 'expected ( for function-pointer args'
                module.consume('(')
                result.args = module.parse_arg_map(is_C99=True)
                module.consume(')')
                result['ref'] = 1

        if push_state:
            module.pop_token_state()
        
        return result

    def emit(self, ctx:'EContext'): return self.get_name()

    # apply all ref
    def ref_total(self):
        ref = self['ref'] or 0
        cur = self
        while True:
            edef     = cur.get_def()
            if isinstance(edef, EAlias):
                cur  = edef.to
                ref += cur.ref()
            else:
                break
        return ref
    
    def ref(self):
        ref = self['ref'] or 0
        return ref
    
    # make an identity thats compatible with C99
    # we index based on type to decorator meta, for const/state keywords, sizes, etc.
    # for now, our identity is based on the type itself, and the sizing will be known in meta on EIdent for our type-tokens
    def updated(self):
        ref = self.decorators['ref'] if self.decorators and 'ref' in self.decorators else 0
        ident_name = ' '.join(str(token) for token in self.list) # + ('*' * ref)
        #ident_size = ''
        #if 'dim' in self.decorators:
        #    for sz_int in self.decorators['dim']:
        #        ident_size += '[%s]' % sz_int
        self.ident = ident_name # + ident_size


    def __repr__(self):
        return self.ident
    """
    def is_value(self, decorator, value):
        if not decorator in self.decorators: return value == None
        return self.decorators[decorator] == value
    
    def matches(self, b):
        a = self
        if len(a) != len(b) or a.list != b.list: return -1
        if a.decorators == b.decorators:         return  0
        if a['ref'] != b['ref']:
            return -1 # if refs are different, this is different
        if a['class'] == True and b['class'] == True:
            return 1 # exact matches take ordered precedence
        if a['struct'] == True and b['struct'] == True:
            return 1 # exact matches take ordered precedence
        if a['enum'] == True and b['enum'] == True:
            return 1 # exact matches take ordered precedence
        return 2
    """    
    def __hash__(self):
        return hash(self.ident)

    def __eq__(self, other):
        if isinstance(other, EIdent):
            return self.ident == other.ident
        return False
    
    def __bool__(self):
        return bool(self.ident)
    
    def __len__(self):
        return len(self.list)
    
    def __getitem__(self, index:Union[int, slice, str]):
        if isinstance(index, str):
            if not self.decorators: return None
            return self.decorators[index] if index in self.decorators else None
        elif isinstance(index, slice):
            if len(self.list) == 0:
                return None
            return self.list[index]
        elif index < len(self.list):
            return self.list[index]
        return None
    
    def __iadd__(self, value):
        if isinstance(value, list):
            self.list.extend(value)
        else:
            self.list.append(value)
        return self
    
    def append(self, v):
        return self.__iadd__(v)
    
    def __setitem__(self, index, value):
        if isinstance(index, str):
            if not self.decorators: self.decorators = OrderedDict()
            self.decorators[index] = value
        else:
            self.list[index] = value

    def get_def(self):
        if not self.ident in self.module.defs:
            return None
        mdef = self.module.defs[self.ident]
        return mdef

    def get_name(self):
        mdef = self.get_def()
        assert mdef, 'no definition resolved on EIdent'
        if isinstance(mdef, EClass):
            res = mdef.name # should be same as self.ident
            if self.meta_types:
                for m in self.meta_types:
                    if res: res += '_'
                    res += m.get_name()
        else:
            res = mdef.name
        return res

# Token has a string and line-number
class Token:
    def __init__(self, value, line_num=0):
        self.value = value
        self.line_num = line_num
    def __hash__(self):
        return hash((self.value))
    def __eq__(self, other):
        if isinstance(other, Token):
            return self.value == other.value
        return False
    def split(ch):              return self.value.split(ch)
    def __str__(self):          return self.value
    def __repr__(self):         return f'Token({self.value}, line {self.line_num})'
    def __getitem__(self, key): return self.value[key]
    def __eq__(self, other):
        if isinstance(other, Token): return self.value == other.value
        elif isinstance(other, str): return self.value == other
        return False

# move to ENode.print
def print_enodes(node, indent=0):
    # handle [array]
    if isinstance(node, list):
        for n in node:
            print_enodes(n, indent)
        return
    # handle specific AST Node class now:
    print(' ' * 4 * indent + str(node))
    if isinstance(node, EClass):
        for name, a_members in node.members.items():
            for member in a_members:
                print_enodes(member, indent + 1)
    elif isinstance(node, EMethod):
        for param in node.parameters:
            print(' ' * 4 * (indent + 1) + f'Parameter: {param}')
        for body_stmt in node.body:
            print_enodes(body_stmt, indent + 1)

# needs to work on windows
def contains_main(obj_file):
    try:
        result = subprocess.run(['nm', obj_file], capture_output=True, text=True, check=True)
        for line in result.stdout.splitlines():
            if ' T main' in line:
                return 1
        return 0
    
    except subprocess.CalledProcessError as e:
        print(f'Error running nm: {e}')
        return -1
    except Exception as e:
        print(f'An error occurred: {e}')
        return -1

def install_dir() -> Path:
    global build_root
    install_dir = '%s/install' % build_root
    i = Path(install_dir)
    i.mkdir(parents=True, exist_ok=True)
    return i

def system(command):
    try:
        print('> %s' % command)
        result = subprocess.run(command, shell=True, check=False)
        return result.returncode
    except Exception as e:
        print(f'error executing command: {e}')
        return -1  # Similar to how system() returns -1 on error

def exe_ext():
    system = platform.system()
    if system == 'Windows':
        return 'exe'
    else:
        return ''

def shared_ext():
    if system == 'Windows':
        return 'dll'
    if system == 'Darwin':
        return 'dylib'
    if system == 'Linux':
        return 'so'
    return ''

def folder_path(path):
    p = Path(path)
    return p if p.is_dir() else p.parent

@dataclass
class EContext:
    module: 'EModule'
    method: 'EMember'
    indent_level:int = 0

    def indent(self):
        return '\t' * self.indent_level
    def increase_indent(self):
        self.indent_level += 1
    def decrease_indent(self):
        if self.indent_level > 0:
            self.indent_level -= 1

@dataclass
class ERef(ENode):
    type:      'EIdent' = None
    value:      ENode  = None
    index:      ENode  = None
    def emit(self, ctx:'EContext'):
        type_name = self.type.get_name()
        if isinstance(self.index, ELiteralInt) and self.index.value != 0: # based on if its a C99 type or not, we will add another *
            return '(%s*)&%s[%d]' % (type_name, self.value.name, self.index.value)
        elif not self.index:
            return '(%s*)%s' % (type_name, self.value.name)
        elif not self.value:
            return '(%s*)null' % type_name
        else:
            return '(%s*)&%s[%d]' % (type_name, self.value.name, self.index.value)

@dataclass
class EMember(ENode):
    name:   str
    type:   EIdent   = None
    module:'EModule' = None
    value:  ENode    = None
    parent:'EClass'  = None
    access: str      = None
    imported:bool    = False
    members:OrderedDict = field(default_factory=OrderedDict) # lambda would be a member of a EMethod, in one example
    args:OrderedDict[str,'EMember'] = None
    meta_types:List[ENode] = None
    static:bool = None
    visibility: str = 'extern'
    def emit(self, ctx:EContext):
        if not self.access or self.access == 'const':
            return '%s' % (self.name)
        else:
            return '%s->%s' % (self.access, self.name)


@dataclass
class EMethod(EMember):
    method_type:str = None
    type_expressed:bool = False
    body:EIdent = None
    code:ENode = None


@dataclass
class EClass(EMember):
    model:          'EModel' = None
    inherits:       'EClass' = None
    block_tokens:    EIdent  = None
    def __repr__(self):
        return 'EClass(%s)' % self.name
    def print(self):
        for name, a_members in self.members.items():
            for member in a_members:
                if isinstance(member, EMethod):
                    print('method: %s' % (name))
                    print_enodes(node=member.code, indent=0)


@dataclass
class EStruct(EMember):
    def __repr__(self): return 'EStruct(%s)' % self.name
    def __post_init__(self):
        self.model = models['struct']


@dataclass
class EUnion(EMember):
    def __repr__(self): return 'EUnion(%s)' % self.name
    def __post_init__(self):
        self.model = models['struct']


@dataclass
class EEnum(EMember):
    def __repr__(self): return 'EEnum(%s)' % self.name
    def __post_init__(self):
        self.model = models['signed-32']


@dataclass
class EAlias(EMember):
    to:EMember=None
    def __repr__(self): return 'EAlias(%s -> %s)' % (self.name, self.to[1].name) 

@dataclass
class BuildState:
    none=0
    built=1

def create_symlink(target, link_name):
    if os.path.exists(link_name):
        os.remove(link_name)
    os.symlink(target, link_name)
    

@dataclass
class EImport(ENode):
    name:       str
    source:     str
    includes:   List[str] = None
    cfiles:     List[str] = field(default_factory=list)
    links:      List[str] = field(default_factory=list)
    build_args: List[str] = None
    import_type = 0
    library_exports:list = None

    none=0
    source=1
    library=2
    # header=3 # not likely using; .h's are selected from source
    project=4

    def file_exists(filepath):
        return os.path.exists(filepath)

    def build_project(self, name, url):
        checkout_dir = os.path.join(build_root, 'checkouts', name)
        s = Path(checkout_dir)
        s.mkdir(parents=True, exist_ok=True)
        i = install_dir()
        build_dir = os.path.join(checkout_dir, 'silver-build')
        b = Path(build_dir)

        if not any(s.iterdir()):  # Directory is empty
            find = url.find('@')
            branch = None
            s_url = url
            if find > -1:
                s_url, branch = url[:find], url[find+1:]
            cmd = f'git clone {s_url} {checkout_dir}'
            assert subprocess.run(cmd, shell=True).returncode == 0, 'git clone failure'
            if branch:
                os.chdir(checkout_dir)
                cmd = f'git checkout {branch}'
                assert subprocess.run(cmd, shell=True).returncode == 0, 'git checkout failure'
            b.mkdir(parents=True, exist_ok=True)
        
        if any(s.iterdir()):  # Directory is not empty
            os.chdir(s)
            
            build_success = os.path.exists('%s/silver-token' % build_dir)
            if os.path.exists('silver-init.sh') and not build_success:
                cmd = f'%s/silver-init.sh "%s"' % (os.getcwd(), i)
                assert system(cmd) == 0
        
            is_rust = os.path.exists('Cargo.toml')
            if is_rust:
                rel_or_debug = 'release'
                package_dir = os.path.join(i, 'rust', name)
                package = Path(package_dir)
                package.mkdir(parents=True, exist_ok=True)
                os.environ['RUSTFLAGS'] = '-C save-temps'
                os.environ['CARGO_TARGET_DIR'] = str(package)
                cmd = f'cargo build -p {name} --{rel_or_debug}'
                assert subprocess.run(cmd, shell=True).returncode == 0
                lib = os.path.join(package_dir, rel_or_debug, f'lib{name}.so')
                exe = os.path.join(package_dir, rel_or_debug, f'{name}_bin')
                if not os.path.exists(exe):
                    exe = os.path.join(package_dir, rel_or_debug, name)
                if os.path.exists(lib):
                    sym = os.path.join(i, f'lib{name}.so')
                    self.links = [name]
                    create_symlink(lib, sym)
                if os.path.exists(exe):
                    sym = os.path.join(i, name)
                    create_symlink(exe, sym)
                
            else:
                assert os.path.exists('CMakeLists.txt'), 'CMake required for project builds'

                cmake_flags = ''
                if self.build_args:
                    for arg in self.build_args:
                        if cmake_flags: cmake_flags += ' '
                        cmake_flags += arg
                
                if not build_success:
                    cmd = f'cmake -S . -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON -DCMAKE_POSITION_INDEPENDENT_CODE=ON -B {b} -DCMAKE_INSTALL_PREFIX={i} {cmake_flags}'
                    assert system(cmd) == 0
                    os.chdir(b)
                    assert system('make -j16 install') == 0
                
                # set effective links and verify install
                if not self.links: self.links = [name]
                for name in self.links:
                    lib = os.path.join(i, 'lib', f'lib{name}.so')
                    ext = 'so'
                    if not os.path.exists(lib):
                        lib = os.path.join(i, 'lib', f'lib{name}.a')
                        ext = 'a'
                    assert os.path.exists(lib)
                    sym = os.path.join(i, f'lib{name}.{ext}')
                    create_symlink(lib, sym)

                built_token = open('silver-token', 'w') # im a silver token, i say we seemed to have built before -- if this is not here, we perform silver-init.sh
                built_token.write('')
                built_token.close()
        
        return BuildState.built

    def build_source(self):
        install = install_dir()
        for cfile in self.cfiles:
            cwd = os.getcwd()
            if cfile.endswith('.rs'):
                # rustc integrated for static libs only in this use-case
                compile = 'rustc --crate-type=staticlib -C opt-level=%s %s/%s --out-dir %s' % (
                    '0' if is_debug else '3',
                    cwd, cfile,
                    build_root)
                # needs to know the product of the build here
            else:
                opt = '-g2' if is_debug else '-O2'
                compile = 'gcc -I%s/install/include %s -Wfatal-errors -Wno-write-strings -Wno-incompatible-pointer-types -fPIC -std=c99 -c %s/%s -o %s/%s.o' % (
                    build_root,
                    opt,
                    cwd, cfile,
                    build_root, cfile)

            print('%s > %s\n' % (cwd, compile))
            assert(system(compile) == 0);

        return BuildState.built

    def process(self, module):
        if (self.name and not self.source and not self.includes):
            attempt = ['','spec/']
            exists = False;
            for ia in range(len(attempt)):
                pre = attempt[ia]
                si_path = Path('%s%s.si' % (pre, self.name))
                if not si_path.exists():
                    continue
                self.module_path = si_path
                print('module %s' % si_path)
                self.module = si_path
                exists = True
                break
            
            assert exists, 'path does not exist for silver module: %s' % self.name
        elif self.name and self.source:                        
            has_c  = False
            has_h  = False
            has_rs = False
            has_so = False
            has_a  = False

            for i in range(len(self.source)):
                i0 = self.source[i]
                if i0.endswith('.c'):
                    has_c = True
                    break
                if i0.endswith('.h'):
                    has_h = True
                    break
                if i0.endswith('.rs'):
                    has_rs = True
                    break
                if i0.endswith('.so'):
                    has_so = True
                    break
                if i0.endswith('.a'):
                    has_a = True
                    break
            
            if has_h:
                self.import_type = EImport.source # we must do 1 at a time, currently
            elif has_c or has_rs:
                assert os.chdir(self.relative_path) == 0
                # build this single module, linking all we have imported prior
                self.import_type = EImport.source
                self.build_source(self.name, self.source)
            elif has_so:
                assert os.chdir(self.relative_path) == 0
                # build this single module, linking all we have imported prior
                self.import_type = EImport.library
                if not self.library_exports:
                    self.library_exports = []
                for i in self.source:
                    lib = self.source[i]
                    rem = lib.substr(0, len(lib) - 3)
                    self.library_exports.append(rem)
            elif has_a:
                assert(os.chdir(self.relative_path) == 0);
                # build this single module, linking all we have imported prior
                self.import_type = EImport.library
                if not self.library_exports:
                    self.library_exports = []
                for i in self.source:
                    lib = self.source[i]
                    rem = lib.substr(0, len(lib) - 2)
                    self.library_exports.append(rem)
            else:
                # source[0] is the url, we need to register the libraries we have
                # for now I am making a name relationship here that applies to about 80% of libs
                assert len(self.source) == 1, ''
                self.import_type = EImport.project
                self.build_project(self.name, self.source[0])
                if not self.library_exports:
                    self.library_exports = []
                # only do this if it exists
                self.library_exports.append(self.name)

        # parse all headers with libclang
        module.process_includes(self.includes)


@dataclass
class EContructMethod(EMethod):
    pass


@dataclass
class EConstruct(ENode):
    type: EIdent
    method: EMethod
    args:List[ENode] # when selected, these should all be filled to line up with the definition


@dataclass
class EExplicitCast(ENode):
    type: EIdent
    value: ENode
    def emit(self, ctx:EContext):
        return '(%s)(%s)' % (self.type.emit(ctx), self.value.emit(ctx))
    

@dataclass
class EProp(EMember):
    is_prop:bool = True


@dataclass
class EUndefined(ENode):
    pass


@dataclass
class EParenthesis(ENode):
    type:EIdent
    enode:ENode
    def emit(self, ctx:EContext):
        return '(%s)' % self.enode.emit(ctx)


@dataclass
class ELogicalNot(ENode):
    type:EIdent
    enode:ENode
    def emit(self, ctx:EContext):
        return '!(%s)' % self.enode.emit(ctx)


@dataclass
class EBitwiseNot(ENode):
    type:EIdent
    enode:ENode
    def emit(self, ctx:EContext):
        return '~(%s)' % self.enode.emit(ctx)


@dataclass
class EAssign(ENode):
    type:EIdent
    target:ENode
    value:ENode
    declare:bool = False

    def emit(self, ctx:EContext):
        op = '='
        name = type(self).__name__
        if name != 'EAssign':
            t = name[7:]
            m = {
                'Add':      '+=',
                'Sub':      '-=',
                'Mul':      '*=',
                'Div':      '/=',
                'Or':       '|=',
                'And':      '&=',
                'Xor':      '^=',
                'ShiftR':   '>>=',
                'ShiftL':   '<<=',
                'Mod':      '%='
            }
            assert t in m
            op = m[t]

        if isinstance(self.target, EProp) and self.target.access == 'const':
            assert False, 'all arguments are const in silver [i.e. you cant change the argument]'

        if isinstance(self.target, ERef):
            if self.declare:
                assert False, 'ref declaration with assignment not handled'
                return '%s %s %s %s' % (self.type.emit(ctx), self.target.name, op, self.value.emit(ctx))
            else:
                return    '((*%s) %s %s)' % (self.target.value.name, op, self.value.emit(ctx))
        else:
            
            if self.declare:
                return '%s %s %s %s' % (self.type.emit(ctx), self.target.name, op, self.value.emit(ctx))
            else:
                return    '%s %s %s' % (self.target.name, op, self.value.emit(ctx))
    

@dataclass
class EAssignAdd(EAssign):    pass


@dataclass
class EAssignSub(EAssign):    pass


@dataclass
class EAssignMul(EAssign):    pass


@dataclass
class EAssignDiv(EAssign):    pass


@dataclass
class EAssignOr(EAssign):     pass


@dataclass
class EAssignAnd(EAssign):    pass


@dataclass
class EAssignXor(EAssign):    pass


@dataclass
class EAssignShiftR(EAssign): pass


@dataclass
class EAssignShiftL(EAssign): pass


@dataclass
class EAssignMod(EAssign):    pass


@dataclass
class EIf(ENode):
    type: EIdent # None; can be the last statement in body?
    condition:ENode
    body:'EStatements'
    else_body:ENode
    def emit(self, ctx:EContext):
        e  = ''
        e += ctx.indent() + 'if (%s) {\n' % self.condition.emit(ctx)
        e += self.body.emit(ctx)
        e += ctx.indent() + '}'
        if self.else_body:
            e += ' else {\n'
            e += self.body.emit(ctx)
            e += ctx.indent() + '}\n'
        e += '\n'
        return e


@dataclass
class EFor(ENode):
    type: EIdent # None; we won't allow None Type ops to flow in processing
    init:ENode
    condition:ENode
    update:ENode
    body:ENode


@dataclass
class EWhile(ENode):
    type: EIdent     # None
    condition:ENode
    body:ENode
    def emit(self, ctx:EContext):
        e  = 'while (%s) {\n' % self.condition.emit(ctx)
        e += self.body.emit(ctx)
        e += ctx.indent() + '}'
        e += '\n'
        return e


@dataclass
class EDoWhile(ENode):
    type: EIdent     # None
    condition:ENode
    body:ENode
    def emit(self, ctx:EContext):
        e  = ctx.indent() + 'do {\n'
        e += self.body.emit(ctx)
        e += ctx.indent() + '} while (%s)\n' % self.condition.emit(ctx)
        e += '\n'
        return e


@dataclass
class EBreak(ENode):
    type: EIdent     # None
    def emit(self, ctx:EContext):
        e = 'break'
        return e

@dataclass
class ELiteralReal(ENode):
    type: EIdent     # f64
    value:float
    def emit(self, ctx:EContext): return repr(self.value)


@dataclass
class ELiteralInt(ENode):
    type: EIdent     # i64
    value:int
    def emit(self,  n:EContext):  return str(self.value)
    

@dataclass
class ELiteralStr(ENode):
    type: EIdent     # string
    value:str
    def emit(self, n:EContext): return '"%s"' % self.value[1:len(self.value) - 1]
        
def value_for_type(type, token):
    if type == ELiteralStr:  return token.value # token has the string in it
    if type == ELiteralReal: return float(token.value)
    if type == ELiteralInt:  return int(token.value)


@dataclass
class ELiteralStrInterp(ENode):
    type: EIdent     # string
    value:str
    args:Union[List[ENode] | OrderedDict[str,ENode]] = None
    # todo:
    def emit(self, n:EContext): return '"%s"' % self.value[1:len(self.value) - 1]

@dataclass
class ELiteralBool(ENode):
    type: EIdent
    value:bool
    def emit(self,  n:EContext):  return 'true' if self.value else 'false'

# these EOperator's are emitted only for basic primitive ops
# class operators have their methods called with EMethodCall

@dataclass
class EOperator(ENode):
    type:EIdent
    left:ENode
    right:ENode
    op:str = ''
    def __post_init__(self):
        t = type(self).__name__
        m = {
            'EAdd':      '+',
            'ESub':      '-',
            'EMul':      '*',
            'EDiv':      '/',
            'EOr':       '|',
            'EAnd':      '&',
            'EXor':      '^'
        }
        assert t in m
        self.op = m[t]
        return super().__post_init__()
    def emit(self, ctx:EContext): return self.left.emit(ctx) + ' ' + self.op + ' ' + self.right.emit(ctx)


@dataclass
class EAdd(EOperator):
    pass


@dataclass
class ESub(EOperator):
    pass


@dataclass
class EMul(EOperator):
    pass


@dataclass
class EDiv(EOperator):
    pass


@dataclass
class EOr(EOperator):
    pass


@dataclass
class EAnd(EOperator):
    pass


@dataclass
class EXor(EOperator):
    pass


@dataclass
class EMethodCall(ENode):
    type:EIdent
    target:ENode
    method:ENode
    args:List[ENode]
    arg_temp_members:List[EMember] = None

    def emit(self, ctx:EContext):
        s_args   = ''
        arg_keys = list(self.method.args.keys())
        arg_len  = len(arg_keys)
        assert arg_len == len(self.args) # this is already done above
        for i in range(arg_len):
            if s_args: s_args += ', '
            str_e = self.args[i].emit(ctx)
            s_args += str_e
        return 'call(%s, %s, %s)' % (self.target, self.method.name, s_args)
    

@dataclass
class EMethodReturn(ENode): # v1.0 we will want this to be value:List[ENode]
    type:EIdent
    value:ENode

    def __hash__(self):
        return hash(tuple(_make_hashable(getattr(self, field.name)) for field in fields(self)))
    def emit(self, ctx:EContext):
        return 'return %s' % self.value.emit(ctx)
    

@dataclass
class EStatements(ENode):
    type:EIdent # last statement type
    value:List[ENode]

    def emit(self, ctx:EContext):
        res = ''
        ctx.increase_indent()
        for enode in self.value:
            res += ctx.indent() + enode.emit(ctx) + ';\n'
        ctx.decrease_indent()
        return res

def etype(n):
    return n if isinstance(n, EIdent) else n.type if isinstance(n, ENode) else n


def change_ext(f: str, ext_to: str) -> str:
    base, _ = os.path.splitext(f)
    return f"{base}.{ext_to.lstrip('.')}"

def format_identifier(text):
    u = text.upper()
    f = u.replace(' ', '_').replace(',', '_').replace('-', '_')
    while '__' in f:
        f = f.replace('__', '_')
    return f


@dataclass
class EModule(ENode):
    path:           Path                        = None
    name:           str                         = None
    tokens:         List[Token]                 = None
    include_paths:  List[Path]                  = field(default_factory=list)
    clang_cache:    OrderedDict[str,  object]   = field(default_factory=OrderedDict)
    include_defs:   OrderedDict[str,  object]   = field(default_factory=OrderedDict)
    parent_modules: OrderedDict[str, 'EModule'] = field(default_factory=OrderedDict)
    defs:           OrderedDict[str, 'ENode']   = field(default_factory=OrderedDict)
    type_cache:     OrderedDict[str, 'EIdent']   = field(default_factory=OrderedDict)
    id:             int                         = None
    finished:       bool                        = False
    recur:          int = 0
    clang_defs:     OrderedDict                 = field(default_factory=OrderedDict)

    # moving the parser inside of EModule
    # move to EModule (all of these methods)
    index:int          = 0
    tokens:List[Token] = field(default_factory=list)
    token_bank:list    = field(default_factory=list)

    member_stack:List[OrderedDict[str, EMember]] = field(default_factory=list)

    def eof(self):
        return not self.tokens or self.index >= len(self.tokens)
    
    def read_tokens(self, input_string) -> List[Token]: # should we call Token JR ... or Tolken?
        special_chars:str  = '$,<>()![]/+*:=#'
        tokens:List[Token] = list()
        line_num:int       = 1
        length:int         = len(input_string)
        index:int          = 0

        while index < length:
            char = input_string[index]
            
            if char in ' \t\n\r':
                if char == '\n':
                    line_num += 1
                index += 1
                continue
            
            if char == '#':
                if index + 1 < length and input_string[index + 1] == '#':
                    index += 2
                    while index < length and not (input_string[index] == '#' and index + 1 < length and input_string[index + 1] == '#'):
                        if input_string[index] == '\n':
                            line_num += 1
                        index += 1
                    index += 2
                else:
                    while index < length and input_string[index] != '\n':
                        index += 1
                    line_num += 1
                    index += 1
                continue
            
            if char in special_chars:
                if char == ':' and input_string[index + 1] == ':':
                    tokens.append(Token('::', line_num))
                    index += 2
                else:
                    tokens.append(Token(char, line_num))
                    index += 1
                continue

            if char in ['"', "'"]:
                quote_char = char
                start = index
                index += 1
                while index < length and input_string[index] != quote_char:
                    if input_string[index] == '\\' and index + 1 < length and input_string[index + 1] == quote_char:
                        index += 2
                    else:
                        index += 1
                index += 1
                tokens.append(Token(input_string[start:index], line_num))
                continue

            start = index
            while index < length and input_string[index] not in ' \t\n\r' + special_chars:
                index += 1
            tokens.append(Token(input_string[start:index], line_num))

        return tokens

    def __hash__(self):
        return hash(id(self))

    def __eq__(self, other):
        if isinstance(other, EModule):
            return id(self) == id(other)
        return False

    def find_clang_def(self, name):
        kind = None
        if isinstance(name, tuple):
            kind = name[0]
            name = name[1]
        # we need to lookup the path via our own space + C space
        # since we are defined after, we will lookup ours first
        a = name.split()
        name = a[0]
        search_struct = True
        search_enum   = True
        is_struct     = False
        is_enum       = False
        if a[0] == 'struct':
            name = a[1]
            search_enum = False
            is_struct   = True
        elif a[0] == 'enum':
            name = a[1]
            search_struct = False
            is_enum       = True
        sp = name.split('*')
        name = sp[0]
        refs = len(sp) - 1
        decorators = OrderedDict([('ref', refs), ('struct', is_struct), ('enum', is_enum)])
        tokens = EIdent(tokens=name, kind=kind, decorators=decorators, module=self)
        for header, defs in self.include_defs.items():
            if name in defs:
                return defs[name], tokens
            
    def find_def(self, ident:object): # we have a map on EIdent for class, struct, etc already
        if not isinstance(ident, EIdent):
            ident = EIdent(ident, module=self)

        for iname, enode in self.parent_modules:
            if isinstance(enode, 'EModule'):
                f, tokens = enode.find_def(tokens)
                if f: return f, tokens

        assert ident.ident in self.defs, 'definition not found: %s' % tokens.ident
        edef = self.defs[ident.ident]
        return edef

    def header_emit(self, id, h_module_file = None):#
        # write includes, for each import of silver module (and others that require headers)
        ifdef_id = format_identifier(id)
        file = open(h_module_file, 'w')
        file.write('#ifndef _%s_\n' % ifdef_id)
        file.write('/* generated silver C99 emission for module: %s */\n' % (self.name))
        file.write('#include <A>\n')
        for name, mod in self.parent_modules.items():
            file.write('#include <%s.h>\n' % (name)) # double quotes means the same path as the source

        # for each definition in module:
        for name, cl in self.defs.items():
            if isinstance(cl, EClass):
                # we will only emit our own types
                if cl.model.name != 'allocated': continue
                # write class declaration
                file.write('\n/* class-declaration: %s */\n' % (name))
                file.write('#define %s_meta(X,Y,Z) \\\n' % (name))
                for member_name, a_members in cl.members.items():
                    for member in a_members:
                        if isinstance(member, EProp):
                            # todo: handle public / priv / intern
                            base_type = member.type.base_type()
                            file.write('\ti_public(X,Y,Z, %s, %s)\\\n' % (
                                base_type.name, member.name))
                for member_name, a_members in cl.members.items():
                    for member in a_members:
                        if isinstance(member, EMethod):
                            arg_types = ''
                            for arg_name, a in member.args.items():
                                arg_types += ', %s' % str(a.type.get_def().name)
                                # will need a suffix on extra member functions 
                                # (those defined after the first will get _2, _3, _4)
                            if member_name == 'cast':
                                file.write('\ti_cast(X,Y,Z, %s)\\\n' % (
                                    member.type.get_def().name))
                            else:
                                file.write('\t%s_method(X,Y,Z, %s, %s%s)\\\n' % (
                                    's' if member.static else 'i',
                                    member.type.get_def().name, member.name, arg_types))
                file.write('\n')
                if cl.inherits:
                    file.write('declare_mod(%s, %s)\n' % (cl.name, cl.inherits.name))
                else:
                    file.write('declare_class(%s)\n' % (cl.name))
        file.write('#endif\n')
        
    def emit(self, app_def):
        os.chdir(build_root)
        c_module_file = '%s.c' % self.name
        if not c_module_file: c_module_file = self.name + '.c'
        assert index_of(c_module_file, '.c') >= 0, 'can only emit .c (C99) source.'
        print('emitting to %s/%s' % (os.getcwd(), c_module_file))
        file = open(c_module_file, 'w')
        header = change_ext(c_module_file, 'h')
        self.header_emit(self.name, header)
        file.write('#include <%s.h>\n' % (self.name))
        for name, im in self.defs.items():
            if isinstance(im, EImport) and len(im.includes):
                for inc in im.includes:
                    h_file = inc
                    if not h_file.endswith('.h'): h_file += '.h'
                    file.write('#include <%s>\n' % (h_file))
        file.write('\n')

        for name, cl in self.defs.items():
            if isinstance(cl, EClass):
                # only emit silver-based implementation
                if cl.model.name != 'allocated': continue
                for member_name, a_members in cl.members.items():
                    for member in a_members:
                        if isinstance(member, EMethod):
                            args = ', ' if not member.static and len(member.args) else ''
                            for arg_name, a in member.args.items():
                                if args and args != ', ': args += ', '
                                args += '%s %s' % (
                                    a.type.get_def().name, a.name)
                            file.write('%s %s_%s(%s%s) {\n' % (
                                member.type.get_def().name,
                                cl.name,
                                member.name,
                                '' if member.static else cl.name + ' self',
                                args))
                            file.write(member.code.emit(EContext(module=self, method=member))) # method code should first contain references to the args
                            file.write('}\n')
                file.write('\n')
                if cl.inherits:
                    file.write('define_mod(%s, %s)\n' % (cl.name, cl.inherits.name))
                else:
                    file.write('define_class(%s)\n' % (cl.name))
        
        if app_def:
            file.write("""
int main(int argc, char* argv[]) {
    A_finish_types();
    // todo: parse args and set in app class
    %s main = new(%s);
    return (int)call(main, run);
}""" % (app_def.name, app_def.name))

    def __repr__(self):
        return 'EModule(%s)' % self.name
    
    
    def initialize(self):
        global models
        
        # define primitives first
        m = self
        m.defs['bool']   = EClass(module=m, name='bool', visibility='intern', model=models['boolean-32'])
        m.defs['u8']     = EClass(module=m, name='u8',   visibility='intern', model=models['unsigned-8'])
        m.defs['u16']    = EClass(module=m, name='u16',  visibility='intern', model=models['unsigned-16'])
        m.defs['u32']    = EClass(module=m, name='u32',  visibility='intern', model=models['unsigned-32'])
        m.defs['u64']    = EClass(module=m, name='u64',  visibility='intern', model=models['unsigned-64'])
        m.defs['i8']     = EClass(module=m, name='i8',   visibility='intern', model=models['signed-8'])
        m.defs['i16']    = EClass(module=m, name='i16',  visibility='intern', model=models['signed-16'])
        m.defs['i32']    = EClass(module=m, name='i32',  visibility='intern', model=models['signed-32'])
        m.defs['i64']    = EClass(module=m, name='i64',  visibility='intern', model=models['signed-64'])
        m.defs['f32']    = EClass(module=m, name='f32',  visibility='intern', model=models['real-32'])
        m.defs['f64']    = EClass(module=m, name='f64',  visibility='intern', model=models['real-64'])
        m.defs['f128']   = EClass(module=m, name='f128', visibility='intern', model=models['real-128'])
        m.defs['int']    = m.defs['i32']
        m.defs['num']    = m.defs['i64']
        m.defs['real']   = m.defs['f64']
        m.defs['void']   = EClass(module=m, name='void', visibility='intern', model=models['void'])
        m.defs['cstr']   = EClass(module=m, name='cstr', visibility='intern', model=models['cstr'])
        m.defs['symbol'] = EClass(module=m, name='symbol', visibility='intern', model=models['symbol'])
        m.defs['handle'] = EClass(module=m, name='handle', visibility='intern', model=models['handle'])
        m.defs['none']   = m.defs['void']

        translation_map = {
            'void':                     'none',
            'char':                     'i32', # important distinction in silver.  A-type strings give you unicode int32's
            'signed char':              'i8',
            'short':                    'i16',
            'short int':                'i16',
            'int':                      'i32',
            'long':                     'i32',
            'long long':                'i64',
            'long long int':            'i64',
            '__int64':                  'i64',  # For some Windows compilers
            'unsigned char':            'u8',
            'unsigned short':           'u16',
            'unsigned short int':       'u16',
            'unsigned':                 'u32',
            'unsigned int':             'u32',
            'unsigned long':            'u32',
            'unsigned long long':       'u64',
            'unsigned long long int':   'u64',
            'float':                    'f32',
            'double':                   'f64',
            'long double':              'f128',
            '_Bool':                    'bool',
            'long long':                'num',
            'double':                   'real',
            'void*':                    'handle',
            'char*':                    'cstr',
            'const char*':              'symbol'
        }
        for c_type, our_type in translation_map.items():
            m.defs[c_type] = m.defs[our_type]

        # declare these but do not emit, as they are in A
        m.defs['string'] = EClass(module=m, name='string', visibility='intern', model=models['atype'])

    def process_includes(self, includes):
        if not includes: return
        for inc in includes:
            self.parse_header(inc)

    # instance cached EIdent from tokens or simple class definitions (when not using meta)
    def resolve_type(self, tokens:EIdent):   # example: array::num
        module = self
        prev = tokens
        if isinstance(tokens, EMember):
            if tokens.type:
                return tokens.type
            inst = tokens
            tokens = EIdent(inst, module=self) # technically lose the struct, class, enum decorator
        
        assert isinstance(tokens, EIdent), "resolve_type: expected tokens instance"
        
        cursor = 0
        remain = len(tokens) if isinstance(tokens, list) else 1

        def pull_token():
            nonlocal cursor, remain
            assert cursor < len(tokens)
            remain -= 1
            res = tokens[cursor]
            cursor += 1
            return res

        def resolve() -> EIdent:
            if isinstance(tokens, EAlias):
                last_def = tokens.to[1]
                type   = None
                s_type = None
                while last_def:
                    type = self.resolve_type(last_def)
                return type
            
            if isinstance(tokens, EMember):
                definition = tokens
                key_name = definition.name
                if key_name in self.type_cache: # type-cache is a map cache of the stored EIdent
                    return self.type_cache[key_name]
                type = EIdent(key_name, module=self)
                self.type_cache[key_name] = type
                return type

            key_name   = ''                             # initialize key as empty (sets to the value given by user)
            class_name = pull_token()                   # read token
            class_def  = self.find_def(class_name)      # lookup type in our module
            assert class_def != None                    # its a compilation error if it does not exist

            meta_len = len(class_def.meta_types) if class_def.meta_types else 0 # get length of meta types on the class def
            assert meta_len <= remain, 'meta types mismatch'

            key_name += str(class_name)
            meta_types:List[EIdent] = []
            for i in range(meta_len):
                type_from_arg, k = resolve(k)
                assert(type_from_arg)
                assert(k)
                meta_types += type_from_arg
                key_name += '::'
                key_name += k
            
            # return type if it exists in cache
            assert class_def.module
            if key_name in class_def.module.type_cache: # type-cache is a map cache of the stored EIdent
                return class_def.module.type_cache[key_name]
            
            # create instance of EIdent from class definition and meta types
            type = EIdent(key_name, module=self, meta_types=meta_types)
            class_def.module.type_cache[key_name] = type
            return type

        return resolve()

    def complete(self):
        assert not self.finished, 'EModule: invalid call to complete'
        global build_root
        libraries_used   = ''
        compiled_objects = ''
        has_main         = False
        for idef in self.defs:
            obj = self.defs[idef]
            if isinstance(obj, EImport):
                im = obj
                if im.import_type == EImport.source:
                    for source in im.source:
                        # these are built as shared library only, or, a header file is included for emitting
                        if source.endswith('.rs') or source.endswith('.h'):
                            continue
                        if len(compiled_objects):
                            compiled_objects.append(' ')
                        buf = '%s/%s.o' % (build_root, source)
                        if not has_main and contains_main(buf):
                            has_main = True
                        compiled_objects.append(buf)
                    break;
                elif im.import_type == EImport.library or im.import_type == EImport.project:
                    libraries_used = ' '.join([f'-l {lib}' for lib in im.links])
        
        if compiled_objects:
            rel = folder_path(self.path)
            assert(os.chdir(rel) == 0)
            cwd = os.getcwd()
            out = self.path.with_suffix(exe_ext() if has_main else shared_ext());
            out_stem = out.stem()
            install = install_dir();
            # we need to have a lib paths array
            link = 'gcc %s -L %s %s %s -o %s/%s' % (
                '' if has_main else '-shared',
                install,
                compiled_objects, libraries_used,
                build_root, out_stem)
            print('%s > %s\n', cwd, link)
            assert(system(link) == 0)

    def get_base_type(self, type):
        while True:
            if type.kind == TypeKind.TYPEDEF:
                type = type.get_canonical()
            elif type.kind == TypeKind.ELABORATED:
                type = type.get_named_type()
            else:
                return type

    def type_identity(self, type): # changing this so it wont resolve to underlying
        if isinstance(type, EIdent):
            edef = type.get_def()
            if edef and edef.type: # this would not be what we want for 'method' ... perhaps method needs rtype
                return edef.type
            return type
        if isinstance(type, tuple):
            assert isinstance(type[0], TypeKind), 'invalid data'
            assert isinstance(type[1], str),      'invalid data'
            return EIdent(kind=type[0], tokens=rem_spaces(type[1]), module=self)
        if hasattr(type, 'kind'):
            if type.spelling == 'WGPUInstance':
                type
            if index_of(type.spelling, '(*') >= 0:
                type
            b = self.get_base_type(type)
            if id(b) != id(type):
                if b.kind == TypeKind.FUNCTIONPROTO:
                    b_ident = EIdent(tokens='void*(*)()', kind=b.kind, module=self)
                else:
                    b_ident = EIdent(b.spelling, kind=b.kind, module=self)
            else:
                b_ident = None
            if b.kind == TypeKind.FUNCTIONPROTO:
                return EIdent(tokens='void*(*)()', kind=b.kind, base=b_ident, module=self)
            else:
                return EIdent(tokens=rem_spaces(b.spelling), kind=b.kind, base=b_ident, module=self)
        if 'underlying_type' in type:
            return type['underlying_type']
        if 'name' in type:
            return EIdent(tokens=rem_spaces(type['name']), kind=type['kind'], module=self)
        assert False, f'Unexpected type structure: {type}'

    def function_info(self, cursor):
        function_name = cursor.spelling
        return_type   = cursor.result_type
        params        = [(param.spelling, param.type) for param in cursor.get_arguments()]
        return {
            'kind':        'function',
            'name':        function_name,
            'return_type': self.type_identity(return_type),
            'parameters':  [(name, self.type_identity(type)) for name, type in params]
        }

    def struct_info(self, cursor):
        fields = []
        for f in cursor.get_children():
            if f.kind == CursorKind.FIELD_DECL:
                fields.append((f.spelling, self.type_identity(f.type)))
        return {
            'kind':    'struct',
            'name':     cursor.spelling,
            'fields':   fields
        }
    
    def union_info(self, cursor):
        fields = []
        for f in cursor.get_children():
            if f.kind in [CursorKind.FIELD_DECL, CursorKind.UNION_DECL]:
                fields.append((f.spelling, self.type_identity(f.type)))
        return {
            'kind':    'union',
            'name':     cursor.spelling,
            'fields':   fields
        }
    
    def enum_info(self, cursor):
        enumerators = []
        for enum in cursor.get_children():
            if enum.kind == CursorKind.ENUM_CONSTANT_DECL:
                enumerators.append((enum.spelling, enum.enum_value))
        return {
            'kind': 'enum',
            'name': cursor.spelling,
            'enumerators': enumerators
        }

    def has_function_pointer(self, cursor):
        if cursor.kind != CursorKind.TYPEDEF_DECL: # needs an args check, member check, etc...
            for field in cursor.get_children():
                t = field.type
                if t:
                    b = self.get_base_type(t)
                    if index_of(b.spelling, '*)') >= 0:
                        return True
            return False

        utype = cursor.underlying_typedef_type
        if index_of(utype.spelling, '*)') >= 0:
            return True
        else:
            return False
        
    def typedef_info(self, cursor):
        utype = cursor.underlying_typedef_type
        return {
            'kind':            'typedef',
            'name':             cursor.spelling,
            'underlying_type':  self.type_identity(utype)
        }

    def edef_for(self, clang_def): 

        # there will be cases where 'spelling' wont be there and we have a struct name*
        name = None
        if isinstance(clang_def, EIdent):
            name = clang_def.ident
            kind = clang_def.kind
        elif isinstance(clang_def, tuple):
            assert False, 'not supported'
            name = clang_def[1].ident # deprecate
            kind = clang_def[0]
        else:
            name = clang_def['name'] # deprecate
            kind = clang_def['kind']
        
        ident = EIdent(name, kind=kind, module=self)
        if name in self.defs:
            return self.defs[name], ident # verify we have no others above!
        
        # tokens of a simple type name can now store its multi dimensional attributes, as well as if it comes from enum, struct, etc
        id = ident.ident
        is_unnamed = index_of(name, '(unnamed at') >= 0
        assert is_unnamed or name == id, 'identity mismatch'
        
        if is_unnamed:
            name = 'null'
            ident = EIdent('null', module=self)
        
        if not isinstance(kind, str) and not id in self.defs:
            if not id in self.clang_defs:
                assert id == 'anonymous' or id == 'union', 'case not handled: %s' % id
                return None, None
            assert id in self.clang_defs
            self.edef_for(self.clang_defs[id])
            assert id in self.defs
        
        mdef = self.defs[id] if id in self.defs else None
        if kind == TypeKind.POINTER:
            ref = ident['ref']
            ident['ref'] = ref + 1 if ref else 1
            return mdef, ident
        
        elif kind == TypeKind.ENUM:
            return mdef, ident
        elif kind == TypeKind.RECORD:
            return mdef, ident
        elif kind == TypeKind.VOID:   
            return mdef, ident
        elif kind == 'enum':
            assert not mdef, 'duplicate definition for %s' % name
            self.defs[id] = EEnum(imported=True, name=id, members=OrderedDict(), module=self)
            self.defs[id].type = EIdent(id, module=self)

            enum_def = self.defs[id]
            for enum_name, enum_value in clang_def['enumerators']:
                assert enum_name and enum_value != None
                i32_type = self.resolve_type(self.defs['i32'])
                enum_def.members[enum_name] = EMember(
                    imported=True,
                    name=enum_name,
                    type=i32_type,  # enums don't have a 'type' in the same sense; you might store their value type
                    parent=enum_def,
                    access=None,
                    value=ELiteralInt(type=i32_type, value=enum_value),
                    visibility='extern')
                
        elif kind == 'union':
            self.defs[id] = EUnion(imported=True, name=name, members=OrderedDict(), module=self)
            self.defs[id].type = EIdent(id, kind='union', module=self)

            members = self.defs[id].members
            for m, member_clang_def in clang_def['fields']:
                edef, ftype = self.edef_for(member_clang_def)
                if not edef:
                    continue # on-error-resume-next
                etype = self.resolve_type(ftype)
                members[m] = EMember(
                    imported=True,
                    name=m,
                    type=etype,
                    parent=None,
                    access=None,
                    visibility='extern')
            
        elif kind == 'typedef':
            global debug
            debug += 1
            assert 'underlying_type' in clang_def, 'object mismatch'
            self.defs[id]             = EAlias(imported=True, name=id, to=clang_def['underlying_type'], module=self)
            self.defs[id].type        = EIdent(id, kind='typedef', module=self) # typedefs have no decoration
            self
            
        elif kind == 'struct':
            self.defs[id]             = EStruct(imported=True, name=id, members=OrderedDict(), module=self)
            self.defs[id].type        = EIdent(id, kind='struct', module=self)

            members = self.defs[id].members
            for struct_m, member_clang_def in clang_def['fields']:
                edef, edef_ident = self.edef_for(member_clang_def) # we need the ref data here too
                if not edef:
                    continue # we cannot resolve anonymous members at the moment; it will need to embed them in the various EMembers such as EUnion and EStruct
                # EIdent is more useful than just refs
                assert edef, 'definition not found' 
                etype = self.resolve_type(edef_ident)
                members[struct_m] = EMember(
                    imported=True, name=struct_m, type=etype,
                    parent=None, access=None, visibility='intern')
            
        elif kind == 'function':
            rtype_edef, rtype_tokens = self.edef_for(clang_def['return_type'])
            rtype_ident = self.resolve_type(rtype_tokens)
            assert rtype_ident, 'Clang translation failed'
            args = OrderedDict()
            for arg_name, arg_type_info in clang_def['parameters']:
                arg_def, arg_type_ident = self.edef_for(arg_type_info)
                args[arg_name] = EMember(
                    imported=True, name=arg_name, type=arg_type_ident,
                    parent=None, access='const', visibility='intern')
            self.defs[id] = EMethod(
                imported=True, static=True, visibility='intern', type=rtype_ident, module=self, name=name, args=args)
        else:
            return None, None

        return self.defs[id], ident


    # parse header with libclang (can use directly in C, as well)
    def parse_header(self, header_file):
        h_key = header_file
        if not header_file.endswith('.h'):
            header_file += '.h'
        index = Index.create()
        header_path = None

        include_paths = []
        include_paths.append(Path(os.path.join(install_dir(), "include/")))
        for inc in self.include_paths:
            include_paths.append(Path(inc))

        for path in include_paths:
            full_path = os.path.join(path, header_file)
            if os.path.exists(full_path):
                header_path = full_path
                break
        assert header_path, f'header-not-found: {header_file}'
        translation_unit = index.parse(header_path, args=['-x', 'c'])

        self.clang_defs = OrderedDict()

        def fn(fcursor):
            for cursor in translation_unit.cursor.walk_preorder():
                name = cursor.spelling
                if name == 'WGPUBufferMapAsyncStatus':
                    name
                if name in self.defs: # handle is in another header, so we'll just keep our definition of it (which comes prior)
                    continue
                if fcursor(cursor):
                    clang_def = None
                    if   cursor.kind == CursorKind.FUNCTION_DECL: clang_def = self.function_info(cursor)
                    elif cursor.kind == CursorKind.STRUCT_DECL:   clang_def = self.struct_info(cursor)
                    elif cursor.kind == CursorKind.ENUM_DECL:     clang_def = self.enum_info(cursor)
                    elif cursor.kind == CursorKind.UNION_DECL:    clang_def = self.union_info(cursor)
                    elif cursor.kind == CursorKind.TYPEDEF_DECL:  clang_def = self.typedef_info(cursor)
                    if clang_def:
                        self.clang_defs[name] = clang_def
                        self.edef_for(clang_def)

        # this wont work if function pointer defs are used inside of args prior to their ordering
        # thats unusual enough for now
        fn(lambda cursor: not self.has_function_pointer(cursor))
        fn(lambda cursor:     self.has_function_pointer(cursor)) # better to add to remainder list

        # create ENode definitions (EClass, EStruct, EMethod, EEnum) from Clang data
        #for name, clang_def in self.clang_defs.items():
        #    self.edef_for(clang_def)
        
        # create EMember for each of the above
        self.include_defs[h_key] = self.clang_defs
        return self.include_defs[h_key]

    def push_token_state(self, new_tokens, index=0):
        self.token_bank.append({'index':self.index, 'tokens':self.tokens})
        self.index = index
        self.tokens = new_tokens

    def transfer_token_state(self):
        tokens_before = self.tokens
        index_before = self.index
        ts = self.token_bank.pop()
        self.index  = index_before
        self.tokens = ts['tokens']
        assert self.tokens == tokens_before, 'transfer of cursor state requires the same list'
        assert self.index  <=  index_before, 'transfer of cursor state requires the same list'

    def pop_token_state(self):
        assert len(self.token_bank), 'stack is empty'
        ts = self.token_bank.pop()
        self.index = ts['index']
        self.tokens = ts['tokens']

    def next_token(self):
        if self.index < len(self.tokens):
            token = self.tokens[self.index]
            self.index += 1
            return token
        return None
    
    def prev_token(self):
        if self.index > 0:
            self.index -= 1
            token = self.tokens[self.index]
            return token
        return None

    def debug_tokens(self):
        return self.tokens, self.index
    
    def peek_token(self, ahead = 0):
        if self.index + ahead < len(self.tokens):
            return self.tokens[self.index + ahead]
        return None
    
    def parse_method(self, cl, method):
        return
    
    def assertion(self, cond, text):
        if not cond:
            print(text)
            exit(1)
    
    # its useful that types are never embedded in other types. theres no reason for extra complexity
    def parse_member_tokens(self):
        tokens = EIdent(module=self)
        while True:
            t = self.next_token()
            assert is_alpha(t.value), 'enodes / parse_type / is_alpha'
            tokens.append(str(t))
            p = self.peek_token()
            if not p.value == '.': break
        return tokens
    
    def peek_member_path(self):
        tokens = EIdent(module=self)
        ahead = 0
        while True:
            t = self.peek_token(ahead)
            if not is_alpha(t.value):
                tokens = EIdent(module=self)
                break
            assert is_alpha(t.value), 'enodes / parse_type / is_alpha'
            tokens.append(str(t))
            ahead += 1
            p = self.peek_token(ahead)
            if not p or not p.value == '.': break
            ahead += 1
        tokens.updated()
        return tokens, ahead
    
    def is_method(self, tokens:EIdent):
        v = self.is_defined(tokens)
        return v if isinstance(v, EMethod) else None
        
    # this wont actually validate the meta ordering which resolve_type subsequently performs
    def is_defined(self, tokens:EIdent):
        if not tokens:
            return None
        tokens_ident = EIdent(tokens, module=self).ident
        if tokens_ident in self.defs:
            type_def = self.defs[tokens_ident]
            return self.defs[tokens_ident]
        else:
            return None
    
    # lookup access to member
    def lookup_member(self, s:str):
        s = str(s)
        for i in range(len(self.member_stack)):
            index = len(self.member_stack) - 1 - i
            map = self.member_stack[index]
            if s in map:
                access = map[s]
                return access
        return None
    
    def member_lookup(self, etype:EIdent, name:str) -> EMember:
        while True:
            cl = etype.get_def()
            if name in cl.members:
                assert isinstance(cl.members[name], list), 'expected list instance holding member'
                return cl.members[name][0]
            if not cl.inherits:
                break
            cl = cl.inherits
        return None
    
    # all Clang types should be ready for resolve_member; which means i dont believe we lazy load the EMember
    def resolve_member(self, member_path:List): # example object-var-name.something-inside
        # we want to output the EIdent, and its EMember
        if member_path:
            f = self.lookup_member(member_path[0])
            if f:
                t = f.type
                m = f
                for token in member_path[1:]:
                    s = str(token)
                    t, m = self.member_lookup(t, s)
                    assert t and m, 'member lookup failed on %s.%s' % (t.get_def().name, s)
                return t, m
        return None, 0
    
    def parse_expression(self):
        return self.parse_add()
    
    def consume(self, peek=None):
        if isinstance(peek, int):
            self.index += peek
        else:
            if peek:
                assert peek == self.peek_token().value, 'expected %s' % str(peek)
            self.next_token()

    def model(self, edef):
        while hasattr(edef, 'to'):
            edef = edef.to.get_def()
        return edef.model if hasattr(edef, 'model') else models['allocated']

    def is_primitive(self, ident):
        e = etype(ident)
        if e.ref(): return True
        edef = e.get_def()
        assert edef
        n = self.model(edef)
        return n.name != 'allocated' and n.name != 'struct' and n.size > 0
    
    # select the larger of realistic vs int, with a preference on realistic numbers so 32bit float is selected over 64bit int
    def preferred_type(self, etype0, etype1):
        etype0 = etype(etype0)
        etype1 = etype(etype1)
        if etype0 == etype1: return etype0
        model0 = etype0.get_def().model
        model1 = etype1.get_def().model
        if model0.realistic:
            if model1.realistic:
                return etype1 if etype1.model.size > model0.size else etype0
            return etype0
        if model1.realistic:
            return etype1
        if model0.size > model1.size:
            return etype0
        return etype1

    def parse_operator(self, parse_lr_fn, op_type0, op_type1, etype0, etype1):
        left = parse_lr_fn()
        while self.peek_token() == op_type0 or self.peek_token() == op_type1:
            etype = etype0 if self.peek_token() == op_type0 else etype1
            self.consume()
            right = parse_lr_fn()
            assert op_type0 in operators, 'operator0 not found'
            assert op_type1 in operators, 'operator1 not found'
            op_name = operators[op_type0 if self.peek_token() == op_type0 else op_type1]
            if op_name in left.type.get_def().members:
                ops = left.type.get_def().members[op_name]
                for method in ops:
                    # check if right-type is compatible
                    assert len(method.args) == 1, 'operators must take in 1 argument'
                    if self.convertible(right.type, method.args[0].type):
                        return EMethodCall(type=method.type, target=left,
                            method=method, args=[self.convert_enode(right, method.args[0].type)])

            left = etype(type=self.preferred_type(left, right), left=left, right=right)
        return left

    def parse_add(self):
        t_state = self.peek_token()
        return self.parse_operator(self.parse_mult,    '+', '-', EAdd, ESub)
    
    def parse_mult(self):
        t_state = self.peek_token()
        return self.parse_operator(self.parse_primary, '*', '/', EMul, EDiv)
    
    def is_bool(self, token):
        t = str(token)
        return ELiteralBool if t in ['true', 'false'] else EUndefined
    
    def is_numeric(self, token):
        is_digit = token.value[0] >= '0' and token.value[0] <= '9'
        has_dot  = index_of(token.value, '.') >= 0
        if not is_digit and not has_dot: return EUndefined
        if not has_dot:                  return ELiteralInt
        return ELiteralReal
    
    def is_string(self, token):
        t = token.value[0]
        if t == '"' or t == '\'': return ELiteralStr
        return EUndefined

    def is_reference(self, token):
        s = str(token)
        for i in range(len(self.member_stack)):
            index = len(self.member_stack) - 1 - i
            map = self.member_stack[index]
            if s in map:
                member = map[s]
                return member # EReference(member=member)
        return EUndefined

    def parse_args(self, signature:EMethod, C99:bool = False): # needs to be given target args
        #b = '(' if C99 else '['
        e = ')' if C99 else ']'
        #self.assertion(self.peek_token() == b, 'expected [ for args found %s' % str(self.peek_token()))
        #self.consume()
        enode_args = []
        arg_count  = -1 if not signature else len(signature.args)
        arg_index  = 0
        if arg_count:
            while (1):
                op = self.parse_expression()
                enode_args.append(op)
                arg_index += 1
                if self.peek_token() == ',':
                    if arg_count > 0 and arg_index >= arg_count:
                        assert False, 'too many args given to %s' % signature.name
                    self.consume()
                else:
                    break
        self.assertion(self.peek_token() == e, 'expected ] after args')
        return enode_args
        
    def type_of(self, value):
        tokens = None
        if isinstance(value, ENode):
            return value.type
        if isinstance(value, str):
            tokens = EIdent(value, module=self)
        assert tokens, 'not implemented'
        type = self.resolve_type(tokens)
        return type

    # List/array is tuple
    def parse_primary(self):
        id = self.peek_token() # may have to peek the entire tokens set for member path
        if not id:
            id = self.peek_token()
        #print('parse_primary: %s' % (id.value))
        # ref keyword allows one to declare [args, not here], assign, or retrieve pointer
        if id == 'ref':
            tokens = EIdent.parse(self)
            member = self.lookup_member(tokens[0])
            indexing_node = None
            if member:
                # this is letting us access or assign the ref on the right
                peek = self.peek_token()
                if peek == '[':
                    self.consume('[')
                    indexing_node = self.parse_expression()
                    self.consume(']')
                t = self.peek_token()
                ref = ERef(type=member.type, value=member, index=indexing_node)
                if t == ':': # handle = consts
                    self.consume(':')
                    r = self.parse_expression()
                    return EAssign(type=member.type, target=ref, value=r)
                else:
                    return ref
            else:
                type = self.resolve_type(tokens)
                t = self.next_token()
                assert t == '[', 'reference must be given a Type[ value ]'
                i = self.parse_expression()
                t = self.peek_token()
                if t == '[':
                    self.consume('[')
                    indexing_node = self.parse_expression()
                    self.consume(']')
                ref = ERef(type=i.type, value=i, index=indexing_node) # indexing baked into the expression value
                if t == ':':
                    self.consume(':')
                    r = self.parse_expression()
                    return EAssign(type=type, target=ref, value=r)
                else:
                    return ref
        
        # suffix operators and methods
        if id == '!' or id == 'not': # cannot overload this operator, it would resolve to not bool operator if defined
            self.consume()
            enode = self.parse_expression()
            return ELogicalNot(type=enode.type, enode=enode)

        if id == '~':
            self.consume()
            enode = self.parse_expression()
            return EBitwiseNot(type=enode.type, enode=enode)

        # typeof[ type ]
        if id == 'typeof':
            self.consume()
            assert self.peek_token() == '[', 'expected [ after typeof'
            self.consume()
            tokens = self.parse_type_ident()
            assert len(tokens), 'expected type after typeof'
            type = self.resolve_type(tokens)
            assert self.peek_token() == ']', 'expected ] after type-identifier'
            self.consume()
            return type

        # casting
        if id == '[': # translate all C types to our own
            self.consume()
            cast_expr = self.parse_expression()
            self.consume() # consume end bracket
            assert self.peek_token() == ']', 'expected closing parenthesis'
            if isinstance(cast_expr, EIdent):
                expr   = self.parse_expression()
                method = self.castable(expr, cast_expr)
                return EMethodCall(type=etype(cast_expr), target=expr, method=method, args=[])
            else:
                return EParenthesis(type=etype(cast_expr), enode=cast_expr) # not a cast expression, just an expression
    
        # check for explicit numerics / strings / bool
        n = self.is_numeric(id)
        if n != EUndefined:
            f = self.next_token()
            type = self.type_of('f64' if n == ELiteralReal else 'i64')
            return n(type=type, value=value_for_type(type=n, token=f))
        
        s = self.is_string(id)
        if s != EUndefined:
            f = self.next_token()
            return s(type=self.type_of('cstr'), value=f.value) # remove the quotes
        
        b = self.is_bool(id)
        if b != EUndefined:
            f = self.next_token()
            return ELiteralBool(type=self.type_of('bool'), value=id == 'true')
        
        member_path, member_count = self.peek_member_path() # we need an actual EMember returned here
        itype, imember = self.resolve_member(member_path)
        # resolve member should be looking at the entire member stack, so we dont need 'is_reference' below
        if itype:
            # check if member is a method
            if isinstance(imember, EMethod) and self.peek_token(member_count) == '[':
                self.consume(member_count + 1)
                #last           = member_path[-1]
                #itype, imember = self.resolve_member([last])
                #type,  member  = self.resolve_member( member_path) # for car.door.open  we want door (target) and open (method)
                enode_args     = self.parse_args(imember)
                self.consume(']')
                conv_args      = self.convert_args(imember, enode_args)
                return EMethodCall(type=itype, target=imember.access, method=imember, args=conv_args)
        else:

            self.push_token_state(self.tokens, self.index)
            tokens = EIdent.parse(self)
            type_def = self.is_defined(tokens)
            if type_def:
                self.transfer_token_state() # this is a pop and index update; it means continue from here (we only do this if we are consuming this resource)
                type = self.resolve_type(tokens)
                token_after = self.peek_token()
                if token_after == '[':
                    self.consume()
                    if self.is_method(tokens):
                        enode_args = self.parse_args(type_def)
                        assert type_def.type, 'type required on EMethod'
                        return EMethodCall(type=type_def.type, target=type_def.access, method=type_def, args=enode_args)
                    else:
                        # construction
                        enode_args = self.parse_args(type.get_def().members['ctr']) # we need to give a signature list of the constructors
                        conv = enode_args
                        method = None
                        # if args are given, we use a type-matched constructor (first arg is primary)
                        if len(enode_args):
                            method = self.constructable(enode_args[0], type)
                            assert method, 'cannot construct with type %s' % enode_args[0].type.get_def().name
                            conv = self.convert_args(method, enode_args)
                        # return construction node; if no method is defined its a simple new and optionally initialized
                        return EConstruct(type=type, method=method, args=conv)
                else:
                    self.consume()
                    return type
            else:
                self.pop_token_state() # we may attempt to parse differently now

            
        # read member stack (may need to go above, and we may need to read member info in the ident reader)
        i = self.is_reference(id) # line: 1085 in example.py: this is where its wrong -- n is defined as a u32 but its not returning reference here
        if i != EUndefined:
            f = self.peek_token()
            self.consume()
            return i # token n skips by this
        if is_alpha(id):
            assert False, 'unknown identifier: %s' % id
        return None
    
    def reset_member_depth(self):
        self.member_stack = [OrderedDict()]
    
    def push_member_depth(self):
        self.member_stack.append(OrderedDict())

    def pop_member_depth(self):
        return self.member_stack.pop()

    def push_member(self, member:EMember, access:EMember = None):
        assert(member.name not in self.member_stack[-1])
        self.member_stack[-1][member.name] = member

    def casts(self, enode_or_etype):
        try:
            return etype(enode_or_etype).get_def().members['cast']
        except:
            return None
            
    def constructs(self, enode_or_etype):
        try:
            return etype(enode_or_etype).get_def().members['construct']
        except:
            return None
        
    # we have 3 types here:
    # EIdent
    #
    # clang-tuple-identity [with TypeKind + EIdent <-- TypeKind not needed if we are using EIdent]
    #

    # can we use EIdent instead of EIdent
    # would prefer this. it can match just fine.
    # E is basically an instance of one that must be truly unique.
    # EIdent you can match against afterwards
    # We can do meta in EIdent.  I am not sure why we didnt
    # once we do that, we have essentially what we need for replacement

    def castable(self, fr:EIdent, to:EIdent):
        fr = etype(fr)
        #to = etype(to)
        assert isinstance(fr, EIdent), 'castable fr != EIdent'
        assert isinstance(to, EIdent), 'castable to != EIdent'
        ref = fr.ref_total()
        if (ref > 0 or self.is_primitive(fr)) and to.get_def().name == 'bool':
            return True
        fr = etype(fr)
        cast_methods = self.casts(fr)
        if fr == to or (self.is_primitive(fr.get_def()) and self.is_primitive(to.get_def())):
            return True
        for method in cast_methods:
            if method.type == to:
                return method
        return None
    
    # constructors are reduced too, first arg as how we match them, the rest are optional args by design
    def constructable(self, fr:EIdent, to:EIdent):
        fr = etype(fr)
        assert isinstance(fr, EIdent), 'constructable fr != EIdent'
        assert isinstance(to, EIdent), 'constructable to != EIdent'
        ctrs = self.constructs(fr)
        if fr == to:
            return True
        for method in ctrs:
            assert len(method.args), 'constructor must have args'
            if method.args[0].type == to:
                return method
        return None

    def convertible(self, efrom, to):
        fr = etype(efrom)
        assert isinstance(efrom, EIdent), 'convertible fr != EIdent'
        assert isinstance(to,    EIdent), 'convertible to != EIdent'
        if fr == etype: return True
        return self.castable(fr, to) or self.constructable(fr, to)

    def convert_enode(self, enode, type):
        assert enode.type, 'code has no type. thats trash code'
        if enode.type != type:
            cast_m      = self.castable(enode, type)
            # type is the same, or its a primitive -> primitive conversion
            if cast_m == True: return EExplicitCast(type=type, value=enode)

            # there is a cast method defined
            if cast_m: return EConstruct(type=type, method=cast_m, args=[enode]) # convert args to List[ENode] (same as EMethod)
            
            # check if constructable
            construct_m = self.constructable(enode, type)
            if construct_m: return EConstruct(type=type, method=construct_m, args=[enode])
            assert False, 'type conversion not found'
        else:
            return enode
        
    def convert_args(self, method, args):
        conv = []
        len_args = len(args)
        len_method = len(method.args)
        assert len_args == len_method, 'arg count mismatch %i (user) != %i (impl)' % (len_args, len_method)
        arg_keys = list(method.args.keys())
        for i in range(len_args):
            u_arg_enode = args[i]
            m_arg_member = method.args[arg_keys[i]]
            assert u_arg_enode.type, 'user arg has no type'
            assert m_arg_member.type, 'method arg has no type'
            if u_arg_enode.type != m_arg_member.type:
                conversion = self.convert_enode(u_arg_enode, m_arg_member.type)
                conv.append(conversion)
            else:
                conv.append(u_arg_enode)
        return conv

    def parse_statement(self):
        t0 = self.peek_token()

        # when you cant think of the whole parser you just do a part of it
        # 'this' could be defined in this var-space 
        # emitting it in initializer function handles the implementation side
        # something.another: convertible[arg:1, arg2:convertible[]]
        
        # if type is found
        type_def = self.is_defined([t0])
        if type_def: # [ array? ]
            if isinstance(type_def, EMethod):
                return self.parse_expression()
            ident  = EIdent.parse(self)
            type   = self.resolve_type(ident)
            name   = None
            after_type = self.peek_token()
            s_member_path = [str(ident[0])] # this is incomplete

            # variable declaration, push to stack so we know what it is when referenced
            if after_type == '.': # make sure we dont stick these together anymore; that was a design flop
                # read EIdent from EClass def member (must be static -> supported in 0.44)
                self.consume()
                while True:
                    part = self.next_token()
                    assert is_alpha(name), 'members are alpha-numeric identifier'
                    s_member_path.append(str(part))
                    if self.peek_token() != '.':
                        break
                after_type = self.next_token()
                # we may be calling a method, returning a property into void (no-op for that, possible error)
                # we may be taking a property and assigning a value to it (do we have to allow for that)
            
            if after_type == '[':
                if len(s_member_path) == 1:
                    # do not construct anonymous instances
                    assert False, 'compilation error: unassigned instance'
                else:
                    # call method
                    method = self.lookup_member(s_member_path)
                    args = self.parse_args(method) # List[ENode]
                    conv = self.convert_args(method, args)

                    # args must be the same count and convertible, type-wise
                    return EMethodCall(target=type, method=method, args=conv)
            else:
                if is_alpha(after_type):
                    # do we need access node or can we put access parent into EMember?
                    # access:None == local, lexical access
                    member = EMember(
                        name=str(after_type), access=None, type=type, value=None, visibility='extern')
                    self.consume()
                    after  = self.peek_token()
                    assign = is_assign(after)
                    self.push_member(member)
                    if assign:
                        self.consume()
                        return EAssign(type=member.type, target=member, declare=True, value=self.parse_expression())
                    else:
                        return member

        elif is_alpha(t0):
            # and non-keyword and non-function, would be an isolated variable perhaps so we can return that if its an acessible member
            # this case is likely not handled right
            # that may be handled in expression, though
            # will need guards on assignment when we are expression level > 0
            return self.parse_expression()
        
            t1 = self.peek_token(1) # this will need to allow for class static
            assign = t1 == ':'
            if assign:
                self.next_token()  # Consume '='
                member = self.lookup_member(str(t0))
                assert member, 'member lookup failed: %s' % (str(t0))
                return EAssign(type=member.type, target=member, value=self.parse_expression())
            elif t1 == '[':
                self.next_token()
                self.next_token()  # Consume '['

                is_static = False  # Placeholder for actual static check

                # this is wrong. we are losing context on our t0 method

                return self.parse_expression()  # Placeholder return for further parsing
            else:
                return self.parse_expression()
            # can be a type, or a variable
        
        elif t0 == 'return':
            self.next_token()  # Consume 'return'
            result = self.parse_expression()
            return EMethodReturn(type=self.type_of(result), value=result)
        
        elif t0 == 'break':
            self.next_token()  # Consume 'break'
            levels = None
            if self.peek_token() == '[':
                self.next_token()  # Consume '['
                levels = self.parse_expression()
                assert self.peek_token() == ']', "expected ']' after break[expression...]"
                self.next_token()  # Consume ']'
            return EBreak(type=None, levels=levels)
        
        elif t0 == 'for':
            self.next_token()  # Consume 'for'
            assert self.peek_token() == '[', "expected condition expression '['"
            self.next_token()  # Consume '['
            self.push_member_depth()
            statement = self.parse_statements()
            assert self.next_token() == ';', "expected ';'"
            condition = self.parse_expression()
            assert self.next_token() == ';', "expected ';'"
            post_iteration = self.parse_expression()
            assert self.next_token() == ']', "expected ']'"
            # i believe C inserts another level here with for; which is probably best
            self.push_member_depth()
            for_block = self.parse_statements()
            self.pop_member_depth()
            self.pop_member_depth()
            return EFor(type=None, init=statement, condition=condition, update=post_iteration, body=for_block)
        
        elif t0 == 'while':
            self.next_token()  # Consume 'while'
            assert self.peek_token() == '[', "expected condition expression '['"
            self.next_token()  # Consume '['
            condition = self.parse_expression()
            assert self.next_token() == ']', "expected condition expression ']'"
            self.push_member_depth()
            statements = self.parse_statements()
            self.pop_member_depth()
            return EWhile(type=None, condition=condition, body=statements)
        
        elif t0 == 'if':
            # with this member space we can declare inside the if as a variable which casts to boolean
            self.push_member_depth()
            self.next_token()  # Consume 'if'
            use_parens = self.peek_token() == '['
            assert not use_parens or self.peek_token() == '[', "expected condition expression '['"
            self.next_token()  # Consume '['
            condition = self.parse_expression()
            assert not use_parens or self.next_token() == ']', "expected condition expression ']'"
            self.push_member_depth()
            statements = self.parse_statements()
            self.pop_member_depth()
            else_statements = None
            if self.peek_token() == 'else':
                self.next_token()  # Consume 'else'
                self.push_member_depth()
                if self.peek_token() == 'if':
                    self.next_token()  # Consume 'else if'
                    else_statements = self.parse_statements()
                else:
                    else_statements = self.parse_statements()
                self.pop_member_depth()
            self.pop_member_depth()
            return EIf(type=None, condition=condition, body=statements, else_body=else_statements)
        
        elif t0 == 'do':
            self.next_token()  # Consume 'do'
            self.push_member_depth()
            statements = self.parse_statements()
            self.pop_member_depth()
            assert self.next_token() == 'while', "expected 'while'"
            assert self.peek_token() == '[', "expected condition expression '['"
            self.next_token()  # Consume '['
            condition = self.parse_expression()
            assert self.next_token() == ']', "expected condition expression ']'"
            return EDoWhile(type=None, condition=condition, body=statements)
        
        else:
            return self.parse_expression()

    def parse_statements(self):
        block = []  # List to hold enode instances
        multiple = self.peek_token() == '['

        tokens, index = self.debug_tokens()
        if multiple:
            self.next_token()  # Consume '['

        depth = 1
        self.push_member_depth()
        while self.peek_token():
            t = self.peek_token()
            if t == '[':
                depth += 1
                self.push_member_depth()
                self.consume()
                continue
            n = self.parse_statement()  # Parse the next statement
            assert n is not None, 'expected statement or expression'
            block.append(n)
            if not multiple: break
            if self.peek_token() == ']':
                self.pop_member_depth()
                self.next_token()  # Consume ']'
                depth -= 1
                if depth == 0:
                    break
        # Return a combined operation of type EType_Statements
        return EStatements(type=None, value=block)
    
    def parse_arg_map(self, is_no_args=False, is_C99=False):
        b = '(' if is_C99 else '['
        e = ')' if is_C99 else ']'
        args:OrderedDict[str, EProp] = OrderedDict()
        if self.peek_token() == e:
            return args
        arg_type_token = None if is_no_args else EIdent.peek(self)
        if is_C99:
            is_C99
        n_args = 0
        while arg_type_token:
            arg_type_token = EIdent.parse(self)
            assert arg_type_token, 'failed to parse type in arg'
            arg_name_token = str(n_args) if is_C99 else None
            if not is_C99 and is_alpha(self.peek_token()):
                arg_name_token = self.next_token()
            if not is_C99:
                assert is_alpha(arg_name_token), 'arg-name (%s) read is not an identifier' % (arg_name_token)
            arg_name = str(arg_name_token)
            n2 = self.peek_token()
            arg_type = self.resolve_type(arg_type_token)
            assert arg_type, 'arg could not resolve type: %s' % (arg_type_token[0].value)
            args[arg_name] = EProp(
                name=str(arg_name_token), access='const',
                type=arg_type, value=None, visibility='extern')
            n = self.peek_token()
            if n == ',':
                self.next_token()  # Consume ','
            else:
                assert n.value == e
            n = self.peek_token()
            if not n in ['ref', 'const', 'class', 'struct', 'enum', 'union'] and not is_alpha(n):
                break
            n_args += 1
        return args

    def finish_method(self, cl, token, method_type, is_static):
        is_ctr  = method_type == 'ctr'
        is_cast = method_type == 'cast'
        is_no_args = method_type in ('init', 'dealloc', 'cast')
        if is_cast: self.consume('cast')
        #prev_token()
        t = self.peek_token()
        has_type = is_alpha(t)
        return_type = EIdent.parse(self) if has_type else EIdent(value=cl.name, module=self) # read the whole type!
        name_token = self.next_token() if (method_type == 'method') else None
        if not has_type:
            self.consume(method_type)
        t = self.next_token() if not is_no_args else '['
        assert t == '[', f"Expected '[', got {token.value} at line {token.line_num}"
        args = self.parse_arg_map(is_no_args)
        if not is_no_args:
            assert self.next_token() in [']', '::'], 'expected end of args'

        t = self.peek_token()
        assert t == '[', f"Expected '[', got {t.value} at line {t.line_num}"
        # parse method body
        body_token = self.next_token()
        body = []
        depth = 1
        while depth > 0:
            body.append(body_token)
            body_token = self.next_token()
            if body_token == '[': depth += 1
            if body_token == ']':
                depth -= 1
                if depth == 0:
                    body.append(body_token)
                
        #else:
        #   body = parse_expression_tokens -- do this in 1.0, however its difficult to lazy load this without error
        
        rtype = self.resolve_type(return_type)
        if method_type == 'method':
            id = str(name_token)
        else:
            id = method_type
        #elif is_cast:
        #    id = 'cast_%s' % str(return_type[0])
        #else:
        #    id = method_type

        if is_cast:
            name = 'cast_%s' % str(return_type[0])
        elif is_ctr:
            name = 'ctr_%s' % str(args[0].type.identifier.name)
        else:
            name = id
        method = EMethod(
            name=name, type=rtype, method_type=method_type, value=None, access='self', parent=cl, type_expressed=has_type,
            static=is_static, args=args, body=body, visibility='extern')
    
        if not id in cl.members:
            cl.members[id] = [method] # we may also retain the with_%s pattern in A-Type
        else:
            cl.members[id].append(method)

        return method

    def finish_class(self, cl):
        if not cl.block_tokens: return
        self.push_token_state(cl.block_tokens)
        self.consume('[')
        cl.members = OrderedDict()
        cl.members['ctr']  = []
        cl.members['cast'] = []
        while (token := self.peek_token()) != ']':
            is_static = token == 'static'
            if is_static:
                self.consume('static')
                token = self.peek_token()
            visibility = 'extern'
            if str(token) in ('extern', 'intern'):
                visibility = str(token)
                token = self.next_token()
            if not is_static:
                is_static = token == 'static'
                if is_static:
                    self.consume('static')
                    token = self.peek_token()
            
            offset = 1
            if token == 'cast':
                assert not is_static, 'cast must be defined as non-static'
                offset += 1
            tokens = EIdent.peek(self)
            is_method0 = tokens and self.peek_token(offset + (len(tokens) * 2 - 1) if tokens else 1) == '['
            global no_arg_methods
            if token.value in no_arg_methods: is_method0 = True # cast and init have no-args

            if not self.is_defined([Token(value='bool')]):
                self.is_defined([Token(value='bool')])

            if not is_method0:
                # property
                tokens0 = self.parse_type_ident()
                assert len(tokens) == len(tokens0), 'type misread (internal)'
                type = self.resolve_type(tokens=tokens)
                
                name_token  = self.next_token()
                next_token_value = self.peek_token().value
                if next_token_value == ':':
                    self.next_token()  # Consume the ':'
                    value_token = self.next_token()
                    prop_node = EProp(
                        type=type, name=name_token.value, access='self',
                        value=value_token.value, visibility=visibility, parent=cl)
                else:
                    prop_node = EProp(
                        type=type, name=name_token.value, access='self',
                        value=None, visibility=visibility, parent=cl)
                id = str(name_token)
                assert not id in cl.members, 'member exists' # we require EMethod != EProp checks too
                cl.members[id] = [prop_node]
            elif token == cl.name:
                self.finish_method(cl, token, 'ctr', is_static)
            elif str(token) in no_arg_methods:
                self.finish_method(cl, token, str(token))
            elif token == 'cast':
                self.finish_method(cl, token, 'cast', is_static)
            elif self.is_defined([token]):
                self.finish_method(cl, token, 'method', is_static)
            else:
                assert False, 'could not parse class method around token %s' % (str(token))
        self.pop_token_state()

    def next_is(self, value):
        return self.peek_token() == value
    
    def convert_literal(self, token):
        assert(self.is_string(token) == ELiteralStr)
        result = token.value[1: 1 + len(token.value) - 2]
        return result
    
    def import_list(self, im, list):
        res = []
        setattr(im, list, res)
        if self.peek_token() == '[':
            self.consume('[')
            while True:
                arg = self.next_token()
                if arg == ']': break
                assert self.is_string(arg) == ELiteralStr, 'expected build-arg in string literal'
                res.append(self.convert_literal(arg))
                if self.peek_token() == ',':
                    self.consume(',')
                    continue
                break
            assert self.next_token() == ']', 'expected ] after build flags'
        else:
            res.append(self.convert_literal(self.next_token()))
        return res

    def parse_import_fields(self, im):
        while True:
            if self.peek_token() == ']':
                self.next_token()
                break
            arg_name = self.next_token()
            if self.is_string(arg_name) == ELiteralStr:
                im.source = [str(arg_name)]
            else:
                assert is_alpha(arg_name), 'expected identifier for import arg'
                assert self.next_token() == ':', 'expected : after import arg (argument assignment)'
                if arg_name == 'name':
                    token_name = self.next_token()
                    assert not self.is_string(token_name), 'expected token for import name'
                    im.name = str(token_name)
                elif arg_name == 'links':    self.import_list(im, 'links')
                elif arg_name == 'includes': self.import_list(im, 'includes')
                elif arg_name == 'source':   self.import_list(im, 'source')
                elif arg_name == 'build':    self.import_list(im, 'build_args')
                elif arg_name == 'shell':
                    token_shell = self.next_token()
                    assert self.is_string(token_shell), 'expected shell invocation for building'
                    im.shell = str(token_shell)
                elif arg_name == 'defines':
                    # none is a decent name for null.
                    assert False, 'not implemented'
                else:
                    assert False, 'unknown arg: %s' % arg_name.value

                if self.peek_token() == ',':
                    self.next_token()
                else:
                    n = self.next_token()
                    assert n == ']', 'expected comma or ] after arg %s' % str(arg_name)
                    break

    def parse_import(self) -> 'EImport':
        result = EImport(name='', source=[], includes=[])
        assert self.next_token() == 'import', 'expected import'

        is_inc = self.peek_token() == '<'
        if is_inc:
            self.consume('<')
        module_name = self.next_token()
        assert is_alpha(module_name), "expected module name identifier"

        if is_inc:
            result.includes.append(str(module_name))
            while True:
                t = self.next_token()
                assert t == ',' or t == '>', 'expected > or , in <include> syntax' % str(t)
                if t == '>': break
                proceeding = self.next_token()
                result.includes.append(str(proceeding))
        as_ = self.peek_token()
        if as_ == 'as':
            self.consume()
            result.isolate_namespace = self.next_token()
        assert is_alpha(module_name), 'expected variable identifier, found %s' % module_name.value
        result.name = str(module_name)
        if as_ == '[':
            self.next_token()
            n = self.peek_token()
            s = self.is_string(n)
            if s == ELiteralStr:
                result.source = []
                while True:
                    inner = self.next_token()
                    assert(self.is_string(inner) == ELiteralStr)
                    source = inner.value[1: 1 + len(inner.value) - 2]
                    result.source.append(source)
                    e = self.next_token()
                    if e == ',':
                        continue;
                    assert e == ']'
                    break
            else:
                self.parse_import_fields(result)

        return result

    def parse_class(self):
        token = self.next_token()
        assert token == 'class', f"Expected 'class', got {token.value} at line {token.line_num}"
        name_token = self.next_token()
        if 'app' in self.defs:
            'found an app'
        is_ty = self.is_defined(name_token)
        assert not is_ty, 'duplicate definition in module'

        assert name_token is not None
        class_node = EClass(module=self, name=name_token.value)

        # set 'model' of class -- this is the storage model, with default being an allocation (heap)
        # model handles storage and primitive logic
        # for now only allocated classes have 
        read_block = True
        if self.peek_token() == '::':
            self.consume()
            model_type = str(self.next_token())
            assert model_type in models, 'model type not found: %s' % (model_type)
            class_node.model = models[model_type]
            read_block = model_type == 'allocated'
        else:
            class_node.model = models['allocated']

        if self.peek_token() == ':':
            self.consume()
            class_node.inherits = self.find_def(self.next_token())
        
        if read_block:
            block_tokens = [self.next_token()]
            assert block_tokens[0] == '[', f"Expected '[', got {token.value} at line {token.line_num}"
            level = 1
            while level > 0:
                token = self.next_token()
                if token == '[': level += 1
                if token == ']': level -= 1
                block_tokens.append(token)
            class_node.block_tokens = block_tokens
        
        return class_node

    # we want type-var[init] to allow for construction with variable types (same as python)
    # in this way we support the meta types easier

    # how do i use this state, push a new one, parse statements and then pop. i suppose i know
    def translate(self, class_def:ENode, method:EMethod):
        print('translating method %s::%s\n' % (class_def.name, method.name))
        assert len(method.body) > 0
        # since translate is inside the read_module() method we must have separate token context here
        self.push_token_state(method.body)
        self.reset_member_depth()
        # when we parse our class, however its runtime we dont need to call the method; its on the instance
        for name, a_members in class_def.members.items():
            for member in a_members:
                self.push_member(member) # if context name not given, we will perform memory management (for args in our case)
        self.push_member_depth()
        for name, member in method.args.items():
            self.push_member(member)
        # we need to push the args in, as well as all class members (prior, so args are accessed first)
        enode = self.parse_statements()
        assert isinstance(enode, EStatements), 'expected statements'
        
        # self return is automatic -- however it might be nice to only do this when the type is not expressed by the user
        if (not method.type_expressed and method.type 
            and method.type.get_def() == class_def
            and isinstance(enode.value[len(enode.value) - 1], EMethodReturn)):
            enode.value.append(EMethodReturn(type=method.type, value=self.lookup_member('self')))

        self.pop_token_state()
        print('printing enodes for method %s' % (method.name))
        print_enodes(enode)
        return enode
    
    def parse(self, tokens:List[Token]):
        self.push_token_state(tokens)
        # top level module parsing
        while self.index < len(self.tokens):
            token = self.next_token()
            if token == 'class':
                self.index -= 1  # Step back to let parse_class handle 'class'
                class_node = self.parse_class()
                assert not class_node.name in self.defs, '%s duplicate definition' % (class_node.name)
                self.defs[class_node.name] = class_node
                continue

            elif token == 'import':
                self.index -= 1  # Step back to let parse_class handle 'class'
                import_node = self.parse_import()
                assert not import_node.name in self.defs, '%s duplicate definition' % (import_node.name)
                self.defs[import_node.name] = import_node
                import_node.process(self)
                continue

        #self.finish_clang_imports()
        module = self
        # finish parsing classes once all identities are known
        for name, enode in self.defs.items():
            if isinstance(enode, EClass): self.finish_class(enode)
        self.pop_token_state()
    
    def __post_init__(self):
        if not self.tokens:
            assert self.path, 'no path given'
            if not self.name:
                self.name = self.path.stem
            f = open(self.path, 'r')
            f_text   = f.read()
            self.tokens = self.read_tokens(f_text)
        if not self.include_paths:
            global _default_paths
            self.include_paths = _default_paths

        self.initialize()              # define primitives first
        self.parse(self.tokens)        # parse tokens
        self.complete()                # compile things imported, before we translate

        # now we graph the methods
        for name, enode in self.defs.items():
            if isinstance(enode, EClass):
                for name, a_members in enode.members.items():
                    for member in a_members:
                        if isinstance(member, EMethod):
                            member.code = self.translate(class_def=enode, method=member)
                            print('method: %s' % name)
                            print_enodes(member.code)
            elif isinstance(enode, EImport):
                pass # 

# all of the basic types come directly from A-type implementation
# string, array, map, hash-map etc

# silver44 (0.4.4) is python, but 1.0.0 will be C (lol maybe)

build_root    = os.getcwd()
first         = sys.argv[1]
module        = EModule(path=Path(first)) # path is relative from our build root (cwd)
app_key       = EIdent('app', module=module)
app           = module.defs['app'] if 'app' in module.defs else None

if app: app.print()

module.emit(app_def=app)