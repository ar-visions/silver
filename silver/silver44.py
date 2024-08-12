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

# non-plain Tokens are defined as ones that have combined these keywords into a decoration map
consumables = ['ref', 'struct', 'class', 'enum', 'const', 'volatile', 'signed', 'unsigned', 'dim', 'import'] # countof is used for i32[4] == 4

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

_next_id = 0  # Class variable to keep track of the next available ID
_default_paths = ['/usr/include/', '/usr/local/include']

def index_of(a, e):
    try:
        return a.index(e)
    except ValueError:
        return -1

def remove_spaces(text):
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

def hashify(cls):
    #def __hash__(self):
    #    return hash(tuple(_make_hashable(getattr(self, field.name)) for field in fields(self)))
    
    #cls.__hash__ = __hash__
    return cls

def make_hashable(item):
    if isinstance(item, list):
        return tuple(make_hashable(sub_item) for sub_item in item)
    elif isinstance(item, dict):
        return tuple(sorted((make_hashable(k), make_hashable(v)) for k, v in item.items()))
    elif isinstance(item, set):
        return frozenset(make_hashable(sub_item) for sub_item in item)
    return item

# tokens can be decorated with any data
@hashify
@dataclass
class Tokens:
    decorators: OrderedDict     = field(default_factory=OrderedDict)
    list:       List['Token']   = field(default_factory=list)
    ident:      str             = None
    module:    'EModule'        = None

    def __init__(self, tokens: Union[str, 'EMember', 'Token', List['Token']] = None, decorators:OrderedDict = OrderedDict(), module:'EModule' = None):
        self.module = module
        assert module, "module must be given to Tokens, as we lookup types here for use in decorating"
        self.decorators = decorators or OrderedDict()
        if isinstance(tokens, Tokens):
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
            tokens          = read_plain_tokens(tokens, module)
            tokens, _       = module.peek_type_tokens(tokens, 0)
            self.decorators = tokens.decorators.copy() if tokens.decorators else OrderedDict()
            self.list       = tokens.list.copy() if tokens.list else []
        else:
            self.list = []
        self.updated()

    # make an identity thats compatible with C99
    # we index based on type to decorator meta, for const/state keywords, sizes, etc.
    # for now, our identity is based on the type itself, and the sizing will be known in meta on Tokens for our type-tokens
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
        if isinstance(other, Tokens):
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
        else:
            return self.list[index]
    
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

def read_plain_tokens(input_string, module:'EModule'):
    special_chars = '$,<>()![]/+*:=#'
    tokens = Tokens(module=module)
    line_num = 1
    length = len(input_string)
    index = 0

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

@hashify
class ENode:
    def __post_init__(self):
        global _next_id
        self.id = _next_id
        _next_id += 1
    
    def __repr__(self):
        attrs = ', '.join(f'{key}={value!r}' for key, value in vars(self).items())
        return f'{type(self).__name__}({attrs})'
    
    def enodes(self):
        return [], [] # key or value? 
    
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

    def each(self, of_type):
        en = self.enodes()
        res = []
        for enode in en:
            if isinstance(enode, of_type):
                res.append(enode)
        return res
    
    def enodes(self) -> List['ENode']:
        enodes = []
        for f in fields(self):
            inst = getattr(self, f.name)
            if isinstance(inst, ENode):
                enodes += [inst]
            elif isinstance(inst, List[ENode]):
                for e in inst:
                    enodes += [e]
            elif isinstance(inst, OrderedDict[str, ENode]):
                for key in inst:
                    enodes += [inst[key]]
        return enodes

    def __eq__(self, other):
        if not isinstance(other, ENode):
            return False
        return self.id == other.id

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
        print(f'Error executing command: {e}')
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

@hashify
@dataclass
class ERef(ENode):
    type:      'EType' = None
    value:      ENode  = None
    index:      ENode  = None

@hashify
@dataclass
class EType(ENode):
    name:str
    definition: 'EMember'
    tokens: Tokens # reference depth
    meta_types: List['EType'] = None
    def get_name(self):
        res = self.definition.name
        if self.meta_types:
            for m in self.meta_types:
                if res: res += '_'
                res += m.get_name()
        return res
    def emit(self, ctx:EContext): return self.get_name()

@hashify
@dataclass
class EMember(ENode):
    name: str
    type: EType      = None
    type_tokens:Tokens = None
    module:'EModule' = None
    value:  ENode    = None
    parent:'EClass'  = None
    access: str      = None
    imported:bool    = False
    types:OrderedDict[Tokens, 'EType'] = field(default_factory=OrderedDict)
    members:OrderedDict = None # lambda would be a member of a EMethod, in one example
    args:OrderedDict[str,'EMember'] = None
    meta_types:List[ENode] = None
    static:bool = None
    visibility: str = 'extern'
    def emit(self, ctx:EContext): return '%s->%s' % (self.access, self.name) if self.access else self.name

@hashify
@dataclass
class EMethod(EMember):
    method_type:str = None
    type_expressed:bool = False
    body:Tokens = None
    code:ENode = None

@hashify
@dataclass
class EClass(EMember):
    model:          'EModel' = None
    inherits:       'EClass' = None
    block_tokens:    Tokens  = None
    def __repr__(self):
        return 'EClass(%s)' % self.name
    def print(self):
        for name, a_members in self.members.items():
            for member in a_members:
                if isinstance(member, EMethod):
                    print('method: %s' % (name))
                    print_enodes(node=member.code, indent=0)

@hashify
@dataclass
class EStruct(EMember):
    def __repr__(self): return 'EStruct(%s)' % self.name
    
@hashify
@dataclass
class EFunctionPointer(EMember):
    def __repr__(self): return 'EStruct(%s)' % self.name

@hashify
@dataclass
class EEnum(EMember):
    def __repr__(self): return 'EEnum(%s)' % self.name

@hashify
@dataclass
class EAlias(EMember):
    to:EMember=None
    def __repr__(self): return 'EAlias(%s -> %s)' % (self.name, self.to.name) 

@hashify
@dataclass
class EModule(ENode):
    path:           Path                        = None
    name:           str                         = None
    tokens:         Tokens                      = None
    include_paths:  List                        = field(default_factory=list)
    clang_cache:    OrderedDict[str,  object]   = field(default_factory=OrderedDict)
    include_defs:   OrderedDict[str,  object]   = field(default_factory=OrderedDict)
    parent_modules: OrderedDict[str, 'EModule'] = field(default_factory=OrderedDict)
    defs:           OrderedDict[str, 'ENode']   = field(default_factory=OrderedDict)
    type_cache:     OrderedDict[str, 'EType']   = field(default_factory=OrderedDict)
    id:             int                         = None
    finished:       bool                        = False

    def __hash__(self):
        return hash(id(self))

    def __eq__(self, other):
        if isinstance(other, EModule):
            return id(self) == id(other)
        return False
    
    def __post_init__(self):
        if not self.tokens:
            assert self.path, 'no path given'
            if not self.name:
                self.name = self.path.stem
            f = open(self.path, 'r')
            f_text   = f.read()
            self.tokens = read_plain_tokens(f_text, self)
        if not self.include_paths:
            global _default_paths
            self.include_paths = _default_paths

        self.initialize()              # define primitives first
        self.parse(self.tokens)        # parse tokens
        self.complete()                # compile things imported

    # best to put the parsing logic in EModule; it was sure nice having all complexity 
    # contained in the parser def, but you cannot access those methods without a call into it, 
    # and then it has caching inside..

    def peek_type_tokens(self, tokens:Tokens, offset=0):
        module = self
        if isinstance(tokens, str):
            t = Tokens(module=self)
            for token in tokens.split():
                t += Token(value=token)
            tokens = t
        def peek_token(offset):
            nonlocal tokens
            return tokens[offset]
        result = Tokens(module=self)
        ahead = offset
        proceed = True
        while proceed:
            proceed = False
            global consumables
            for con in consumables:
                if peek_token(ahead) == con:
                    result[con] = 1
                    ahead += 1
                    proceed = True
                    if ahead >= len(tokens):
                        return result, ahead - offset
        ref = 0
        while True:
            t = peek_token(ahead)
            is_extend = t.value in ['long', 'short', 'unsigned', 'signed']
            is_ext = t.value in ['unsigned', 'signed']
            if not is_extend and not is_alpha(t.value):
                result = Tokens(module=self)
                ahead = offset
                break
            result.list.append(t)
            ahead += 1
            if ahead >= len(tokens):
                break
            p = peek_token(ahead)
            while (p == '*'):
                #result.append(p) -- refs are part of the model
                ref += 1
                ahead += 1
                if ahead >= len(tokens):
                    break
                p = peek_token(ahead)
            if ahead < len(tokens):
                p = peek_token(ahead)
                if p == '[':
                    dim = []
                    while (p == '['):
                        ahead += 1
                        p = peek_token(ahead)
                        if p == ']':
                            v = -1
                            break
                        v = peek_token(ahead)
                        ahead += 1
                        assert peek_token(ahead) == ']', 'expected ] character in C dimensional array declaration'
                        ahead += 1
                        dim.append(int(v.value))
                        assert str(int(v.value) == v.value), 'incorrect numeric format given'
                        if ahead >= len(tokens):
                            break
                        p = peek_token(ahead)
                    result['dim'] = dim
            if ref: break
            if not p or (not p.value in ['::'] and not is_ext): break # this keyword requires single token types for now
            ahead += 1
            if ahead >= len(tokens):
                result = None
                ahead  = offset
                break
        if ref > 0: # ref string* list  would be string**; we are now allowing *'s in the EType identity with ref:True == *
            result['ref'] = (1 + result['ref']) if 'ref' in result else ref
        result.updated()
        return result, ahead - offset
    
    def parse(self, tokens:Tokens):
        parse_tokens(module=self, _tokens=tokens)
    
    def find_clang_def(self, name):
        if isinstance(name, tuple):
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
        tokens = Tokens(tokens=name, decorators=decorators, module=self)
        for header, defs in self.include_defs.items():
            if name in defs:
                return defs[name], tokens

    def get_tokens(self, type_name:object):
        if isinstance(type_name, Tokens): return type_name
        Tokens
            
    def find_def(self, type_tokens:object): # we have a map on Tokens for class, struct, etc already
        orig = type_tokens
        if not isinstance(type_tokens, Tokens):
            type_tokens = Tokens(type_tokens, module=self)

        for iname, enode in self.parent_modules:
            if isinstance(enode, 'EModule'):
                f, tokens = enode.find_def(type_tokens)
                if f: return f, tokens

        assert type_tokens.ident in self.defs, 'definition not found: %s' % type_tokens.ident
        edef = self.defs[type_tokens.ident]
        return edef, edef.type_tokens
    
    def emit(self, app_def):
        os.chdir(build_root)
        c_module_file = '%s.c' % self.name
        if not c_module_file: c_module_file = self.name + '.c'
        assert index_of(c_module_file, '.c') >= 0, 'can only emit .c (C99) source.'
        print('emitting to %s/%s' % (os.getcwd(), c_module_file))
        file = open(c_module_file, 'w')
        header = change_ext(c_module_file, 'h')
        header_emit(self, self.name, header)
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
                            args = ', ' if not member.static else ''
                            for arg_name, a in member.args.items():
                                if args: args += ', '
                                args += '%s %s' % (
                                    a.type.definition.name, a.name)
                            file.write('%s %s_%s(%s%s) {\n' % (
                                member.type.definition.name,
                                cl.name,
                                member.name,
                                '' if member.static else cl.name + ' self',
                                args))
                            file.write(member.code.emit(EContext(module=self, method=member))) # method code should first contain references to the args
                            file.write('}\n')
                file.write('\n')
                if cl.inherits:
                    file.write('define_mod(%s, %s)\n' % (cl.name, cl.inherits))
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
        m.defs[Tokens(module=self, tokens='string')] = EClass(module=m, name='string', visibility='intern', model=models['atype'])

    def process_includes(self, includes):
        if not includes: return
        for inc in includes:
            self.parse_header(inc)

    # instance cached EType from tokens or simple class definitions (when not using meta)
    def resolve_type(self, tokens:Tokens):   # example: array::num
        module = self
        prev = tokens
        if isinstance(tokens, EMember):
            if tokens.type:
                return tokens.type
            inst = tokens
            tokens = Tokens(inst, module=module) # technically lose the struct, class, enum decorator
        
        assert isinstance(tokens, Tokens), "resolve_type: expected tokens instance"
        
        cursor = 0
        remain = len(tokens) if isinstance(tokens, list) else 1

        def pull_token():
            nonlocal cursor, remain
            assert cursor < len(tokens)
            remain -= 1
            res = tokens[cursor]
            cursor += 1
            return res

        def resolve() -> Tuple[EType, str]:
            nonlocal remain
            nonlocal tokens, cursor
            nonlocal module

            if isinstance(tokens, EAlias):
                last_def = tokens.to
                type   = None
                s_type = None
                while last_def:
                    type = module.resolve_type(last_def)
                return type
            
            if isinstance(tokens, EMember):
                definition = tokens
                key_name = definition.name
                if key_name in module.type_cache: # type-cache is a map cache of the stored EType
                    return module.type_cache[key_name], key_name
                type = EType(name=key_name, tokens=Tokens(key_name, module=module), definition=definition)
                module.type_cache[key_name] = type
                return type

            key_name   = ''                             # initialize key as empty (sets to the value given by user)
            class_name = pull_token()                   # read token
            class_def, class_tokens = module.find_def(class_name)    # lookup type in our module
            assert class_def != None                    # its a compilation error if it does not exist

            meta_len = len(class_def.meta_types) if class_def.meta_types else 0 # get length of meta types on the class def
            assert meta_len <= remain, 'meta types mismatch'

            key_name += str(class_name)
            meta_types:List[EType] = []
            for i in range(meta_len):
                type_from_arg, k = resolve(k)
                assert(type_from_arg)
                assert(k)
                meta_types += type_from_arg
                key_name += '::'
                key_name += k
            
            # return type if it exists in cache
            assert class_def.module
            if key_name in class_def.module.type_cache: # type-cache is a map cache of the stored EType
                return class_def.module.type_cache[key_name], key_name
            
            # create instance of EType from class definition and meta types
            type = EType(name=key_name, tokens=Tokens(key_name, module=module), definition=class_def, meta_types=meta_types)
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

    def type_identity(self, type):
        if isinstance(type, tuple):
            return type
        if hasattr(type, 'kind'):
            base_type = self.get_base_type(type)
            return base_type.kind, Tokens(tokens=remove_spaces(base_type.spelling), module=self)
        if 'underlying_type' in type:
            return type['underlying_type'], Tokens(tokens=remove_spaces(type['underlying_type'][1]), module=self)
        if 'name' in type:
            return type['kind'], Tokens(tokens=remove_spaces(type['name']), module=self)
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
        for field in cursor.get_children():
            if field.kind == CursorKind.FIELD_DECL:
                fields.append((field.spelling, self.type_identity(field.type)))
        return {
            'kind': 'struct',
            'name': cursor.spelling,
            'fields': fields
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

    def typedef_info(self, cursor):
        underlying_type = cursor.underlying_typedef_type
        return {
            'kind': 'typedef',
            'name': cursor.spelling,
            'underlying_type': self.type_identity(underlying_type)
        }

    # parse header with libclang (can use directly in C, as well)
    def parse_header(self, header_file):
        module = self
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

        clang_defs = OrderedDict()
        for cursor in translation_unit.cursor.walk_preorder():
            name = cursor.spelling
            if name in module.defs: # handle is in another header, so we'll just keep our definition of it (which comes prior)
                continue
            assert not name in module.defs, 'C99 headers: duplicate definition: %s' % name
            if cursor.kind == CursorKind.FUNCTION_DECL:
                clang_defs[name] = self.function_info(cursor)
            elif cursor.kind == CursorKind.STRUCT_DECL:
                clang_defs[name] = self.struct_info(cursor)
            elif cursor.kind == CursorKind.ENUM_DECL:
                clang_defs[name] = self.enum_info(cursor)
            elif cursor.kind == CursorKind.TYPEDEF_DECL:
                clang_defs[name] = self.typedef_info(cursor)

        def edef_for(clang_def): 
            # there will be cases where 'spelling' wont be there and we have a struct name*
            name = None
            if isinstance(clang_def, tuple):
                name = clang_def[1].ident
                kind = clang_def[0]
            else:
                name = clang_def['name']
                kind = clang_def['kind']

            name_tokens = Tokens(name, module=self)

            if name in module.defs:
                return module.defs[name], name_tokens # verify we have no others above!
            
            # tokens of a simple type name can now store its multi dimensional attributes, as well as if it comes from enum, struct, etc
            id = name_tokens.ident
            is_unnamed = index_of(name, '(unnamed at') >= 0
            assert is_unnamed or name == id, 'identity mismatch'
            if is_unnamed:
                name = 'null'
                name_tokens= Tokens('null', module=self)
            
            # most of these TypeKind explicits we dont need; we can form the type from reading it
            #if kind == TypeKind.CONSTANTARRAY:
            #    module.defs[name_tokens] = EAlias(imported=True, name=name, to=OrderedDict(), module=module)

            if not isinstance(kind, str) and not id in module.defs:
                assert id in clang_defs
                edef_for(clang_defs[id])
                assert id in module.defs
                
            mdef = module.defs[id] if id in module.defs else None
            if kind == TypeKind.POINTER:
                ref = name_tokens['ref']
                name_tokens['ref'] = ref + 1 if ref else 1
                return mdef, name_tokens
            elif kind == TypeKind.ENUM:   return mdef, name_tokens
            elif kind == TypeKind.RECORD: return mdef, name_tokens
            elif kind == TypeKind.VOID:   return mdef, name_tokens
            elif kind == 'enum':
                assert not mdef, 'duplicate definition for %s' % name
                module.defs[id] = EEnum(imported=True, name=name, members=OrderedDict(), module=module)
                enum_def = module.defs[id]
                for child in clang_def['enumerators']:
                    enum_name  = child[0]
                    enum_value = child[1]  # this gets the value assigned to the enum
                    enum_def.members[enum_name] = EMember(
                        imported=True,
                        name=enum_name,
                        type=None,  # enums don't have a 'type' in the same sense; you might store their value type
                        parent=enum_def,
                        access=None,
                        value=ELiteralInt(type=module.resolve_type(module.defs['i32']), value=enum_value),
                        visibility='intern')
            elif kind == 'typedef':
                assert 'underlying_type' in clang_def, 'object mismatch'
                to_type, to_type_tokens = edef_for(clang_def['underlying_type'])
                module.defs[id] = EAlias(imported=True, name=name, to=to_type, module=module)
            elif kind == 'struct':
                module.defs[id] = EStruct(imported=True, name=name, members=OrderedDict(), module=module)
                members = module.defs[id].members
                for struct_m, member_clang_def in clang_def['fields']:
                    if member_clang_def[1].ident == 'WGPUSType':
                        member_clang_def
                    edef, tokens = edef_for(member_clang_def) # we need the ref data here too
                    # tokens is more useful than just refs
                    assert edef, 'definition not found' 
                    etype = self.resolve_type(tokens)
                    members[struct_m] = EMember(
                        imported=True,
                        name=struct_m,
                        type=etype,
                        parent=None,
                        access=None,
                        visibility='intern')
            elif kind == 'function':
                rtype_clang, rtype_tokens = edef_for(clang_def['return_type'])
                etype = module.resolve_type(rtype_tokens)
                assert etype, 'Clang translation failed'
                args = OrderedDict()
                for arg_name, arg_type_info in clang_def['parameters']:
                    arg_def, arg_tokens = edef_for(arg_type_info)
                    # char != i32 because its going to source -- we may want to continue to use typedef aliasing and not shortcut
                    #assert arg_tokens.ident == arg_def.name, 'name mismatch'
                    arg_etype = EType(name=arg_def.name, definition=arg_def, tokens=arg_tokens)
                    args[arg_name] = EMember(
                        imported=True,
                        name=arg_name,
                        type=arg_etype,
                        parent=None,
                        access=None,
                        visibility='intern')
                module.defs[id] = EMethod(
                    imported=True, static=True, visibility='intern', type=etype, name=name, args=args)
            else:
                return None, None

            return module.defs[id], name_tokens
        
        # create ENode definitions (EClass, EStruct, EMethod, EEnum) from Clang data
        for name, clang_def in clang_defs.items():
            edef_for(clang_def)
        
        # create EMember for each of the above
        module.include_defs[h_key] = clang_defs
        return module.include_defs[h_key]
    
@dataclass
class BuildState:
    none=0
    built=1

def create_symlink(target, link_name):
    if os.path.exists(link_name):
        os.remove(link_name)
    os.symlink(target, link_name)
    
@hashify
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
        
@hashify
@dataclass
class EModel(ENode):
    name:str
    size:int
    integral:bool
    realistic:bool
    type:Type
    id:int = None

class HeapType:
    placeholder: bool

class AType:
    placeholder: bool

class Void:
    pass

models:OrderedDict[str, EModel] = OrderedDict()
models['atype']       = EModel(name='atype',       size=8, integral=0, realistic=0, type=AType)
models['allocated']   = EModel(name='allocated',   size=8, integral=0, realistic=0, type=HeapType)
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

@hashify
@dataclass
class EContructMethod(EMethod):
    pass

@hashify
@dataclass
class EConstruct(ENode):
    type: EType
    method: EMethod
    args:List[ENode] # when selected, these should all be filled to line up with the definition

@hashify
@dataclass
class EExplicitCast(ENode):
    type: EType
    value: ENode
    def emit(self, ctx:EContext):
        return '(%s)(%s)' % (self.type.emit(ctx), self.value.emit(ctx))
    
@hashify
@dataclass
class EProp(EMember):
    def emit(self, ctx:EContext): return '%s->%s' % (self.access, self.name) if self.access else self.name

@hashify
@dataclass
class EUndefined(ENode):
    pass

@hashify
@dataclass
class EParenthesis(ENode):
    type:EType
    enode:ENode
    def emit(self, ctx:EContext):
        return '(%s)' % self.enode.value.emit(ctx)

@hashify
@dataclass
class EAssign(ENode):
    type:EType
    target:ENode
    value:ENode
    declare:bool = False

    def emit(self, ctx:EContext):
        op = ''
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
        if self.declare:
            return '%s %s %s= %s' % (self.type.emit(ctx), self.target.name, op, self.value.emit(ctx))
        else:
            return    '%s %s= %s' % (op, self.target.name, self.value.emit(ctx))
    
@hashify
@dataclass
class EAssignAdd(EAssign):    pass

@hashify
@dataclass
class EAssignSub(EAssign):    pass

@hashify
@dataclass
class EAssignMul(EAssign):    pass

@hashify
@dataclass
class EAssignDiv(EAssign):    pass

@hashify
@dataclass
class EAssignOr(EAssign):     pass

@hashify
@dataclass
class EAssignAnd(EAssign):    pass

@hashify
@dataclass
class EAssignXor(EAssign):    pass

@hashify
@dataclass
class EAssignShiftR(EAssign): pass

@hashify
@dataclass
class EAssignShiftL(EAssign): pass

@hashify
@dataclass
class EAssignMod(EAssign):    pass

@hashify
@dataclass
class EIf(ENode):
    type: EType # None; can be the last statement in body?
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

@hashify
@dataclass
class EFor(ENode):
    type: EType # None; we won't allow None Type ops to flow in processing
    init:ENode
    condition:ENode
    update:ENode
    body:ENode

@hashify
@dataclass
class EWhile(ENode):
    type: EType     # None
    condition:ENode
    body:ENode
    def emit(self, ctx:EContext):
        e  = 'while (%s) {\n' % self.condition.emit(ctx)
        e += self.body.emit(ctx)
        e += ctx.indent() + '}'
        e += '\n'
        return e

@hashify
@dataclass
class EDoWhile(ENode):
    type: EType     # None
    condition:ENode
    body:ENode
    def emit(self, ctx:EContext):
        e  = ctx.indent() + 'do {\n'
        e += self.body.emit(ctx)
        e += ctx.indent() + '} while (%s)\n' % self.condition.emit(ctx)
        e += '\n'
        return e

@hashify
@dataclass
class EBreak(ENode):
    type: EType     # None
    def emit(self, ctx:EContext):
        e = 'break'
        return e
@hashify
@dataclass
class ELiteralReal(ENode):
    type: EType     # f64
    value:float
    def emit(self, ctx:EContext): return repr(self.value)

@hashify
@dataclass
class ELiteralInt(ENode):
    type: EType     # i64
    value:int
    def emit(self,  n:EContext):  return str(self.value)
    
@hashify
@dataclass
class ELiteralStr(ENode):
    type: EType     # string
    value:str
    def emit(self, n:EContext): return '"%s"' % self.value[1:len(self.value) - 1]
        
def value_for_type(type, token):
    if type == ELiteralStr:  return token.value # token has the string in it
    if type == ELiteralReal: return float(token.value)
    if type == ELiteralInt:  return int(token.value)

@hashify
@dataclass
class ELiteralStrInterp(ENode):
    type: EType     # string
    value:str
    args:Union[List[ENode] | OrderedDict[str,ENode]] = None
    # todo:
    def emit(self, n:EContext): return '"%s"' % self.value[1:len(self.value) - 1]

@dataclass
class ELiteralBool(ENode):
    type: EType
    value:bool
    def emit(self,  n:EContext):  return 'true' if self.value else 'false'

# these EOperator's are emitted only for basic primitive ops
# class operators have their methods called with EMethodCall
@hashify
@dataclass
class EOperator(ENode):
    type:EType
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

@hashify
@dataclass
class EAdd(EOperator):
    pass

@hashify
@dataclass
class ESub(EOperator):
    pass

@hashify
@dataclass
class EMul(EOperator):
    pass

@hashify
@dataclass
class EDiv(EOperator):
    pass

@hashify
@dataclass
class EOr(EOperator):
    pass

@hashify
@dataclass
class EAnd(EOperator):
    pass

@hashify
@dataclass
class EXor(EOperator):
    pass

@hashify
@dataclass
class EMethodCall(ENode):
    type:EType
    target:ENode
    method:ENode
    args:List[ENode]
    arg_temp_members:List[EMember] = None
    def enodes(self):
        return self.args
    def emit(self, ctx:EContext):
        s_args   = ''
        arg_keys = list(self.method.args.keys())
        arg_len  = len(arg_keys)
        assert arg_len == len(self.args) # this is already done above
        for i in range(arg_len):
            if s_args: s_args += ', '
            s_args += self.args[i].emit(ctx)
        return 'call(%s, %s, %s)' % (self.target, self.method.name, s_args)
    
@hashify
@dataclass
class EMethodReturn(ENode): # v1.0 we will want this to be value:List[ENode]
    type:EType
    value:ENode
    def enodes(self):
        return [self.value]
    def __hash__(self):
        return hash(tuple(_make_hashable(getattr(self, field.name)) for field in fields(self)))
    def emit(self, ctx:EContext):
        return 'return %s' % self.value.emit(ctx)
    
@hashify
@dataclass
class EStatements(ENode):
    type:EType # last statement type
    value:List[ENode]
    def enodes(self):
        return self.value
    def emit(self, ctx:EContext):
        res = ''
        ctx.increase_indent()
        for enode in self.value:
            res += ctx.indent() + enode.emit(ctx) + ';\n'
        ctx.decrease_indent()
        return res

def etype(n):
    return n if isinstance(n, EType) else n.type if isinstance(n, ENode) else n

def parse_tokens(*, module:EModule, _tokens:List['Token']):
    index = 0
    tokens = _tokens
    token_bank = []
    member_stack:List[OrderedDict[str, EMember]] = []  # we must push to this for available members in class, lambda, etc

    def push_token_state(new_tokens):
        nonlocal index, tokens
        token_bank.append({'index':index, 'tokens':tokens})
        index = 0
        tokens = new_tokens

    def pop_token_state():
        nonlocal index, tokens
        ts = token_bank.pop()
        index = ts['index']
        tokens = ts['tokens']

    def next_token():
        nonlocal index, tokens
        if index < len(tokens):
            token = tokens[index]
            index += 1
            print('next token: %s' % token.value)
            return token
        return None
    
    def prev_token():
        nonlocal index, tokens
        if index > 0:
            index -= 1
            token = tokens[index]
            return token
        return None
    
    def peek_token(ahead = 0):
        nonlocal index, tokens
        if index + ahead < len(tokens):
            return tokens[index + ahead]
        return None
    
    def parse_method(cl, method):
        return
    
    def assertion(cond, text):
        if not cond:
            print(text)
            exit(1)
    
    # its useful that types are never embedded in other types. theres no reason for extra complexity
    def parse_member_tokens():
        tokens = Tokens(module=module)
        while True:
            t = next_token()
            assert is_alpha(t.value), 'enodes / parse_type / is_alpha'
            tokens.append(str(t))
            p = peek_token()
            if not p.value == '.': break
        return tokens
    
    def peek_member_path():
        tokens = Tokens(module=module)
        ahead = 0
        while True:
            t = peek_token(ahead)
            if not is_alpha(t.value):
                tokens = Tokens(module=module)
                break
            assert is_alpha(t.value), 'enodes / parse_type / is_alpha'
            tokens.append(str(t))
            p = peek_token(ahead + 1)
            if not p or not p.value == '.': break
            ahead += 2
        return tokens
    
    def parse_member_tokens_2():
        tokens = Tokens(module=module)
        ctx = None
        while True:
            t = next_token()
            if ctx == None:
                member = member_lookup()
                    
            assert is_alpha(t.value), 'enodes / parse_type / is_alpha'
            tokens.append(str(t))
            p = peek_token()
            if not p.value == '.': break
        return tokens
    
    # this wont actually validate the meta ordering which resolve_type subsequently performs
    def is_type(tokens:Tokens):
        if not tokens:
            return None
        return Tokens(tokens, module=module).ident in module.defs
    
    # lookup access to member
    def lookup_member(s:str):
        s = str(s)
        for i in range(len(member_stack)):
            index = len(member_stack) - 1 - i
            map = member_stack[index]
            if s in map:
                access = map[s]
                return access
        return None
    
    def member_lookup(t:EType, name:str) -> Tuple[EType, EMember]:
        while True:
            cl = t.definition
            if name in cl.members:
                return t, cl.members[name][0]
            if not cl.inherits:
                break
            cl = cl.module.find_def(cl.inherits)
        return None, None
    
    # all Clang types should be ready for resolve_member; which means i dont believe we lazy load the EMember
    def resolve_member(member_path:List): # example object-var-name.something-inside
        # we want to output the EType, and its EMember
        f = lookup_member(member_path[0])
        if f:
            # access_name = f.access_name
            t = f.type
            m = f
            for token in member_path[1:]:
                s = str(token)
                t, m = member_lookup(t, s)
                assert t and m, 'member lookup failed on %s.%s' % (t.definition.name, s)
            return t, m
        
        return None, 0
    
    def parse_expression():
        return parse_add()
    
    def consume(peek=None):
        if peek:
            assert peek == peek_token().value, 'expected %s' % str(peek)
        next_token()

    def is_primitive(enode):
        type = etype(enode)
        return type.definition.model != 'allocated' and type.definition.model.size > 0
    
    # select the larger of realistic vs int, with a preference on realistic numbers so 32bit float is selected over 64bit int
    def preferred_type(etype0, etype1):
        etype0 = etype(etype0)
        etype1 = etype(etype1)
        if etype0 == etype1: return etype0
        model0 = etype0.definition.model
        model1 = etype1.definition.model
        if model0.realistic:
            if model1.realistic:
                return etype1 if etype1.model.size > model0.size else etype0
            return etype0
        if model1.realistic:
            return etype1
        if model0.size > model1.size:
            return etype0
        return etype1

    def parse_operator(parse_lr_fn, op_type0, op_type1, etype0, etype1):
        left = parse_lr_fn()
        while peek_token() == op_type0 or peek_token() == op_type1:
            etype = etype0 if peek_token() == op_type0 else etype1
            consume()
            right = parse_lr_fn()
            assert op_type0 in operators, 'operator0 not found'
            assert op_type1 in operators, 'operator1 not found'
            op_name = operators[op_type0 if peek_token() == op_type0 else op_type1]
            if op_name in left.type.definition.members:
                ops = left.type.definition.members[op_name]
                for method in ops:
                    # check if right-type is compatible
                    assert len(method.args) == 1, 'operators must take in 1 argument'
                    if convertible(right.type, method.args[0].type):
                        return EMethodCall(type=method.type, target=left,
                            method=method, args=[convert_enode(right, method.args[0].type)])

            left = etype(type=preferred_type(left, right), left=left, right=right)
        return left

    def parse_add():
        t_state = peek_token()
        return parse_operator(parse_mult,    '+', '-', EAdd, ESub)
    
    def parse_mult():
        t_state = peek_token()
        return parse_operator(parse_primary, '*', '/', EMul, EDiv)
    
    def is_bool(token):
        t = str(token)
        return ELiteralBool if t in ['true', 'false'] else EUndefined
    
    def is_numeric(token):
        is_digit = token.value[0] >= '0' and token.value[0] <= '9'
        has_dot  = index_of(token.value, '.') >= 0
        if not is_digit and not has_dot: return EUndefined
        if not has_dot:                  return ELiteralInt
        return ELiteralReal
    
    def is_string(token):
        t = token.value[0]
        if t == '"' or t == '\'': return ELiteralStr
        return EUndefined

    def is_reference(token):
        s = str(token)
        for i in range(len(member_stack)):
            index = len(member_stack) - 1 - i
            map = member_stack[index]
            if s in map:
                member = map[s]
                return member # EReference(member=member)
        return EUndefined

    def parse_args(): # needs to be given target args
        assertion(peek_token() == '[', 'expected [ for args')
        consume()
        enode_args = []
        while (1):
            op = parse_expression()
            enode_args.append(op)
            if peek_token() == ',':
                consume()
            else:
                break
        assertion(peek_token() == ']', 'expected ] after args')
        consume()
        return enode_args
        
    def type_of(value):
        tokens = None
        if isinstance(value, ENode):
            return value.type
        if isinstance(value, str):
            tokens = Tokens(Token(value=value, line_num=0))
        assert tokens, 'not implemented'
        type = module.resolve_type(tokens)
        return type

    # List/array is tuple
    def parse_primary():
        id = peek_token() # may have to peek the entire tokens set for member path
        if not id:
            id = peek_token()
        print('parse_primary: %s' % (id.value))

        # ref keyword allows one to declare [args, not here], assign, or retrieve pointer
        if id == 'ref':
            type_tokens = parse_type_tokens()
            member = lookup_member(type_tokens[0])
            indexing_node = None
            if member:
                # this is letting us access or assign the ref on the right
                peek = peek_token()
                if peek == '[':
                    consume('[')
                    indexing_node = parse_expression()
                    consume(']')
                t = peek_token()
                ref = ERef(type=member.type, value=member, index=indexing_node)
                if t == ':': # handle = consts
                    consume(':')
                    r = parse_expression()
                    return EAssign(type=member.type, target=ref, value=r)
                else:
                    return ref
            else:
                type = module.resolve_type(type_tokens)
                t = next_token()
                assert t == '[', 'reference must be given a Type[ value ]'
                i = parse_expression()
                t = peek_token()
                if t == '[':
                    consume('[')
                    indexing_node = parse_expression()
                    consume(']')
                ref = ERef(type=i.type, value=i, index=indexing_node) # indexing baked into the expression value
                if t == ':':
                    consume(':')
                    r = parse_expression()
                    return EAssign(type=type, target=ref, value=r)
                else:
                    return ref
        
        # typeof[ type ]
        if id == 'typeof':
            consume()
            assert peek_token() == '[', 'expected [ after typeof'
            consume()
            type_tokens, token_inc = module.parse_type_tokens(tokens, index)
            index += token_inc
            assert len(type_tokens), 'expected type after typeof'
            type = module.resolve_type(type_tokens)
            assert peek_token() == ']', 'expected ] after type-identifier'
            consume()
            return type

        if id == '[': # translate all C types to our own
            consume()
            cast_expr = parse_expression()
            consume() # consume end bracket
            assert peek_token() == ']', 'expected closing parenthesis'
            if isinstance(cast_expr, EType):
                expr   = parse_expression()
                method = castable(expr, cast_expr)
                return EMethodCall(type=etype(cast_expr), target=expr, method=method, args=[])
            else:
                return EParenthesis(type=etype(cast_expr), enode=cast_expr) # not a cast expression, just an expression
    
        n = is_numeric(id)
        if n != EUndefined:
            f = next_token()
            type = type_of('f64' if n == ELiteralReal else 'i64')
            return n(type=type, value=value_for_type(type=n, token=f))
        
        s = is_string(id)
        if s != EUndefined:
            f = next_token()
            return s(type=type_of('string'), value=f.value) # remove the quotes
        
        b = is_bool(id)
        if b != EUndefined:
            f = next_token()
            return ELiteralBool(type=type_of('bool'), value=id == 'true')
        
        # we may attempt to peek at an entire type signature (will still need to peek against member access, too)
        # contructors require args.. thats why you define them, to construct 'with' things
        # otherwise one can override init; the edge case is doing both, in which case we resolve that issue in code
        t = peek_token()
        type_tokens = peek_type_tokens()
        len_type_tokens = len(type_tokens) * 2 - 1
        if type_tokens and type_tokens[0] == 'WGPUInstanceDescriptor':
            print('desc')

        if is_type(type_tokens):
            type = module.resolve_type(type_tokens)
            if peek_token(len_type_tokens + 1) == '[':
                # construction
                enode_args = parse_args(type.definition.members['ctr']) # we need to give a signature list of the constructors
                conv = enode_args
                method = None
                # if args are given, we use a type-matched constructor (first arg is primary)
                if len(enode_args):
                    method = constructable(enode_args[0], type)
                    assert method, 'cannot construct with type %s' % enode_args[0].type.definition.name
                    conv = convert_args(method, enode_args)
                # return construction node; if no method is defined its a simple new and optionally initialized
                return EConstruct(type=type, method=method, args=conv)
            else:
                return type
        else:
            member_path = peek_member_path() # we need an actual EMember returned here
            len_member_tokens = len(member_path) * 2 - 1
            first = str(member_path[0])
            
            # if method call
            if len_member_tokens > 0 and peek_token(len_member_tokens) == '[':
                for i in range(len_member_tokens):
                    consume()
                last = member_path[-1]
                itype, imember = resolve_member([last])
                type,  member  = resolve_member( member_path) # for car.door.open  we want door (target) and open (method)
                enode_args = parse_args()
                conv_args = convert_args(member, enode_args)
                return EMethodCall(type=type, target=imember.access, method=member, args=conv_args)
        
        # read member stack
        i = is_reference(id) # line: 1085 in example.py: this is where its wrong -- n is defined as a u32 but its not returning reference here
        if i != EUndefined:
            f = peek_token()
            consume()
            return i # token n skips by this
        if is_alpha(id):
            assert False, 'unknown identifier: %s' % id
        return None
    
    def reset_member_depth():
        nonlocal member_stack
        member_stack = [OrderedDict()]
    
    def push_member_depth():
        nonlocal member_stack
        member_stack.append(OrderedDict())

    def pop_member_depth():
        return member_stack.pop()

    def push_member(member:EMember, access:EMember = None):
        nonlocal member_stack
        assert(member.name not in member_stack[-1])
        member_stack[-1][member.name] = member

    def casts(enode_or_etype):
        try:
            return etype(enode_or_etype).definition.members['cast']
        except:
            return None
            
    def constructs(enode_or_etype):
        try:
            return etype(enode_or_etype).definition.members['construct']
        except:
            return None
        
    def castable(fr, to:EType):
        fr = etype(fr)
        cast_methods = casts(fr)
        if fr == to or (is_primitive(fr) and is_primitive(to)):
            return True
        for method in cast_methods:
            if method.type == to:
                return method
        return None
    
    # constructors are reduced too, first arg as how we match them, the rest are optional args by design
    def constructable(fr, to:EType):
        fr = etype(fr)
        ctrs = constructs(fr)
        if fr == to:
            return True
        for method in ctrs:
            assert len(method.args), 'constructor must have args'
            if method.args[0].type == to:
                return method
        return None

    def convertible(efrom, to):
        fr = etype(efrom)
        if fr == etype: return True
        return castable(fr, to) or constructable(fr, to)

    def convert_enode(enode, type):
        assert enode.type, 'code has no type. thats trash code'
        if enode.type != type:
            cast_m      = castable(enode, type)
            # type is the same, or its a primitive -> primitive conversion
            if cast_m == True: return EExplicitCast(type=type, value=enode)

            # there is a cast method defined
            if cast_m: return EConstruct(type=type, method=cast_m, args=[enode]) # convert args to List[ENode] (same as EMethod)
            
            # check if constructable
            construct_m = constructable(enode, type)
            if construct_m: return EConstruct(type=type, method=construct_m, args=[enode])
            assert False, 'type conversion not found'
        else:
            return enode
        
    def convert_args(method, args):
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
                conversion = convert_enode(u_arg_enode, m_arg_member.type)
                conv.append(conversion)
            else:
                conv.append(u_arg_enode)
        return conv

    def parse_statement():
        t0 = peek_token()

        # when you cant think of the whole parser you just do a part of it
        # 'this' could be defined in this var-space 
        # emitting it in initializer function handles the implementation side
        # something.another: convertible[arg:1, arg2:convertible[]]
        
        # if type is found
        if is_type([t0]):
            type_tokens = parse_type_tokens()
            type = module.resolve_type(type_tokens)
            name = None
            after_type = peek_token()
            s_member_path = [str(type_tokens[0])]

            # variable declaration, push to stack so we know what it is when referenced
            if after_type == '.': # make sure we dont stick these together anymore; that was a design flop
                # read EType from EClass def member (must be static -> supported in 0.44)
                consume()
                while True:
                    part = next_token()
                    assert is_alpha(name), 'members are alpha-numeric identifier'
                    s_member_path.append(str(part))
                    if peek_token() != '.':
                        break
                after_type = next_token()
                # we may be calling a method, returning a property into void (no-op for that, possible error)
                # we may be taking a property and assigning a value to it (do we have to allow for that)
            
            if after_type == '[':
                if len(s_member_path) == 1:
                    # do not construct anonymous instances
                    assert False, 'compilation error: unassigned instance'
                else:
                    # call method
                    method = module.lookup_member(s_member_path)
                    args = parse_args() # List[ENode]
                    conv = convert_args(method, args)

                    # args must be the same count and convertible, type-wise
                    return EMethodCall(target=type, method=method, args=conv)
            else:
                if is_alpha(after_type):
                    # do we need access node or can we put access parent into EMember?
                    # access:None == local, lexical access
                    member = EMember(name=str(after_type), access=None, type=type, value=None, visibility='extern')
                    consume()
                    after  = peek_token()
                    assign = is_assign(after)
                    push_member(member)
                    if assign:
                        consume()
                        return EAssign(type=member.type, target=member, declare=True, value=parse_expression())
                    else:
                        return member

        elif is_alpha(t0): # and non-keyword, so this is a variable that must be in stack
            t1 = peek_token(1) # this will need to allow for class static
            assign = t1 == ':'
            if assign:
                next_token()  # Consume '='
                member = lookup_member(str(t0))
                assert member, 'member lookup failed: %s' % (str(t0))
                return EAssign(type=member.type, target=member, value=parse_expression())
            elif t1 == '[':
                next_token()
                next_token()  # Consume '['
                # not parsing args
                # todo: parse arguments
                # ---------------------------------
                is_static = False  # Placeholder for actual static check
                # Type lookup and variable space management would go here
                return parse_expression()  # Placeholder return for further parsing
            else:
                return parse_expression()
            # can be a type, or a variable
        
        elif t0 == 'return':
            next_token()  # Consume 'return'
            result = parse_expression()
            return EMethodReturn(type=type_of(result), value=result)
        
        elif t0 == 'break':
            next_token()  # Consume 'break'
            levels = None
            if peek_token() == '[':
                next_token()  # Consume '['
                levels = parse_expression()
                assert peek_token() == ']', "expected ']' after break[expression...]"
                next_token()  # Consume ']'
            return EBreak(type=None, levels=levels)
        
        elif t0 == 'for':
            next_token()  # Consume 'for'
            assert peek_token() == '[', "expected condition expression '['"
            next_token()  # Consume '['
            push_member_depth()
            statement = parse_statements()
            assert next_token() == ';', "expected ';'"
            condition = parse_expression()
            assert next_token() == ';', "expected ';'"
            post_iteration = parse_expression()
            assert next_token() == ']', "expected ']'"
            # i believe C inserts another level here with for; which is probably best
            push_member_depth()
            for_block = parse_statements()
            pop_member_depth()
            pop_member_depth()
            return EFor(type=None, init=statement, condition=condition, update=post_iteration, body=for_block)
        
        elif t0 == 'while':
            next_token()  # Consume 'while'
            assert peek_token() == '[', "expected condition expression '['"
            next_token()  # Consume '['
            condition = parse_expression()
            assert next_token() == ']', "expected condition expression ']'"
            push_member_depth()
            statements = parse_statements()
            pop_member_depth()
            return EWhile(type=None, condition=condition, body=statements)
        
        elif t0 == 'if':
            # with this member space we can declare inside the if as a variable which casts to boolean
            push_member_depth()
            next_token()  # Consume 'if'
            assert peek_token() == '[', "expected condition expression '['"
            next_token()  # Consume '['
            condition = parse_expression()
            assert next_token() == ']', "expected condition expression ']'"
            push_member_depth()
            statements = parse_statements()
            pop_member_depth()
            else_statements = None
            if peek_token() == 'else':
                next_token()  # Consume 'else'
                push_member_depth()
                if peek_token() == 'if':
                    next_token()  # Consume 'else if'
                    else_statements = parse_statements()
                else:
                    else_statements = parse_statements()
                pop_member_depth()
            pop_member_depth()
            return EIf(type=None, condition=condition, body=statements, else_body=else_statements)
        
        elif t0 == 'do':
            next_token()  # Consume 'do'
            push_member_depth()
            statements = parse_statements()
            pop_member_depth()
            assert next_token() == 'while', "expected 'while'"
            assert peek_token() == '[', "expected condition expression '['"
            next_token()  # Consume '['
            condition = parse_expression()
            assert next_token() == ']', "expected condition expression ']'"
            return EDoWhile(type=None, condition=condition, body=statements)
        
        else:
            return parse_expression()

    def parse_statements():
        block = []  # List to hold enode instances
        multiple = peek_token() == '['

        if multiple:
            next_token()  # Consume '['

        depth = 1
        push_member_depth()
        while peek_token():
            t = peek_token()
            if t == '[':
                depth += 1
                push_member_depth()
                consume()
                continue
            n = parse_statement()  # Parse the next statement
            assert n is not None, 'expected statement or expression'
            block.append(n)
            if not multiple: break
            if peek_token() == ']':
                pop_member_depth()
                next_token()  # Consume ']'
                depth -= 1
                if depth == 0:
                    break
        # Return a combined operation of type EType_Statements
        return EStatements(type=None, value=block)

    def parse_type_tokens():
        nonlocal index
        type_tokens, token_inc = module.peek_type_tokens(tokens, index)
        index += token_inc
        return type_tokens
    
    def peek_type_tokens():
        type_tokens, token_inc = module.peek_type_tokens(tokens, index)
        return type_tokens
    
    def finish_method(cl, token, method_type, is_static):
        is_ctr  = method_type == 'ctr'
        is_cast = method_type == 'cast'
        is_no_args = method_type in ('init', 'dealloc', 'cast')
        if is_cast: consume('cast')
        #prev_token()
        t = peek_token()
        has_type = is_alpha(t)
        return_type = parse_type_tokens() if has_type else [Token(value=cl.name, line_num=0)] # read the whole type!
        name_token = next_token() if (method_type == 'method') else None
        if not has_type:
            consume(method_type)

        t = next_token() if not is_no_args else '['
        assert t == '[', f"Expected '[', got {token.value} at line {token.line_num}"
        arg_type_token = None if is_no_args else peek_type_tokens()
        args:OrderedDict[str, EProp] = OrderedDict()

        if not arg_type_token:
            if not is_no_args:
                t = peek_token()
                assert t == ']', 'expected ]'
                consume()
        else:
            while arg_type_token:
                arg_type_token = parse_type_tokens()
                assert arg_type_token, 'failed to parse type in arg'
                arg_name_token = next_token()
                arg_name = str(arg_name_token)
                assert is_alpha(arg_name_token), 'arg-name (%s) read is not an identifier' % (arg_name_token)
                arg_type = module.resolve_type(arg_type_token)
                assert arg_type, 'arg could not resolve type: %s' % (arg_type_token[0].value)
                args[arg_name] = EProp(name=str(arg_name_token), access=None, type=arg_type, value=None, visibility='extern')
                if peek_token() == ',':
                    next_token()  # Consume ','
                else:
                    nx = peek_token()
                    assert nx.value == ']'
                if peek_token() == ']':
                    consume()
                    break

        t = peek_token()
        assert t == '[', f"Expected '[', got {t.value} at line {t.line_num}"
        # parse method body
        body_token = next_token()
        body = []
        depth = 1
        while depth > 0:
            body.append(body_token)
            body_token = next_token()
            if body_token == '[': depth += 1
            if body_token == ']':
                depth -= 1
                if depth == 0:
                    body.append(body_token)
                
        #else:
        #   body = parse_expression_tokens -- do this in 1.0, however its difficult to lazy load this without error
        
        rtype = module.resolve_type(return_type)
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
            static=is_static, args=args, body=body, visibility='extern') # help here.. so, for access we need to create an EType to self, somehow.  EType is EClass + List[EType] for Meta runtime.  We dont Permute EClasses!  We give them extra Types at Runtime..  Help me figure this one out

        if not id in cl.members:
            cl.members[id] = [method] # we may also retain the with_%s pattern in A-Type
        else:
            cl.members[id].append(method)

        return method

    def finish_class(cl):
        if not cl.block_tokens: return
        push_token_state(cl.block_tokens)
        consume('[')
        cl.members = OrderedDict()
        cl.members['ctr']  = []
        cl.members['cast'] = []
        while (token := peek_token()) != ']':
            is_static = token == 'static'
            if is_static:
                consume('static')
                token = peek_token()
            visibility = 'extern'
            if str(token) in ('extern', 'intern'):
                visibility = str(token)
                token = next_token()
            if not is_static:
                is_static = token == 'static'
                if is_static:
                    consume('static')
                    token = peek_token()
            # next is type token
            offset = 1
            if token == 'cast':
                assert not is_static, 'cast must be defined as non-static'
                offset += 1
            type_tokens = peek_type_tokens()
            is_method = type_tokens and peek_token(offset + (len(type_tokens) * 2 - 1) if type_tokens else 1) == '['
            global no_arg_methods
            if token.value in no_arg_methods: is_method = True # cast and init have no-args



            if not is_type([Token(value='bool')]):
                is_type([Token(value='bool')])


            if not is_method:
                # property
                type_tokens0 = parse_type_tokens()
                assert len(type_tokens) == len(type_tokens0), 'type misread (internal)'
                type = module.resolve_type(tokens=type_tokens)
                
                name_token  = next_token()
                next_token_value = peek_token().value
                if next_token_value == ':':
                    next_token()  # Consume the ':'
                    value_token = next_token()
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
                finish_method(cl, token, 'ctr', is_static)
            elif str(token) in no_arg_methods:
                finish_method(cl, token, str(token))
            elif token == 'cast':
                finish_method(cl, token, 'cast', is_static)
            elif is_type([token]):
                finish_method(cl, token, 'method', is_static)
            else:
                assert False, 'could not parse class method around token %s' % (str(token))
        pop_token_state()

    def next_is(value):
        return peek_token() == value
    
    def convert_literal(token):
        assert(is_string(token) == ELiteralStr)
        result = token.value[1: 1 + len(token.value) - 2]
        return result
    
    def import_list(im, list):
        res = []
        setattr(im, list, res)
        if peek_token() == '[':
            consume('[')
            while True:
                arg = next_token()
                if arg == ']': break
                assert is_string(arg) == ELiteralStr, 'expected build-arg in string literal'
                res.append(convert_literal(arg))
                if peek_token() == ',':
                    consume(',')
                    continue
                break
            assert next_token() == ']', 'expected ] after build flags'
        else:
            res.append(convert_literal(next_token()))
        return res

    def parse_import_fields(im):
        self = im
        while True:
            if peek_token() == ']':
                next_token()
                break
            arg_name = next_token()
            if is_string(arg_name) == ELiteralStr:
                self.source = [str(arg_name)]
            else:
                assert is_alpha(arg_name), 'expected identifier for import arg'
                assert next_token() == ':', 'expected : after import arg (argument assignment)'
                if arg_name == 'name':
                    token_name = next_token()
                    assert not is_string(token_name), 'expected token for import name'
                    self.name = str(token_name)
                elif arg_name == 'links':    import_list(self, 'links')
                elif arg_name == 'includes': import_list(self, 'includes')
                elif arg_name == 'source':   import_list(self, 'source')
                elif arg_name == 'build':    import_list(self, 'build_args')
                elif arg_name == 'shell':
                    token_shell = next_token()
                    assert is_string(token_shell), 'expected shell invocation for building'
                    self.shell = str(token_shell)
                elif arg_name == 'defines':
                    # none is a decent name for null.
                    assert False, 'not implemented'
                else:
                    assert False, 'unknown arg: %s' % arg_name.value

                if peek_token() == ',':
                    next_token()
                else:
                    n = next_token()
                    assert n == ']', 'expected comma or ] after arg %s' % str(arg_name)
                    break

    def parse_import() -> EImport:
        self = EImport(name='', source=[], includes=[])
        assert next_token() == 'import', 'expected import'

        is_inc = peek_token() == '<'
        if is_inc:
            consume('<')
        module_name = next_token()
        assert is_alpha(module_name), "expected module name identifier"

        if is_inc:
            self.includes.append(str(module_name))
            while True:
                t = next_token()
                assert t == ',' or t == '>', 'expected > or , in <include> syntax' % str(t)
                if t == '>': break
                proceeding = next_token()
                self.includes.append(str(proceeding))
        as_ = peek_token()
        if as_ == 'as':
            consume()
            self.isolate_namespace = next_token()
        assert is_alpha(module_name), 'expected variable identifier, found %s' % module_name.value
        self.name = str(module_name)
        if as_ == '[':
            next_token()
            n = peek_token()
            s = is_string(n)
            if s == ELiteralStr:
                self.source = []
                while True:
                    inner = next_token()
                    assert(is_string(inner) == ELiteralStr)
                    source = inner.value[1: 1 + len(inner.value) - 2]
                    self.source.append(source)
                    e = next_token()
                    if e == ',':
                        continue;
                    assert e == ']'
                    break
            else:
                parse_import_fields(self)

        return self

    def parse_class():
        token = next_token()
        assert token == 'class', f"Expected 'class', got {token.value} at line {token.line_num}"
        name_token = next_token()
        if 'app' in module.defs:
            'found an app'
        is_ty = is_type(name_token)
        assert not is_ty, 'duplicate definition in module'

        assert name_token is not None
        class_node = EClass(module=module, name=name_token.value)

        # set 'model' of class -- this is the storage model, with default being an allocation (heap)
        # model handles storage and primitive logic
        # for now only allocated classes have 
        read_block = True
        if peek_token() == '::':
            consume()
            model_type = str(next_token())
            assert model_type in models, 'model type not found: %s' % (model_type)
            class_node.model = models[model_type]
            read_block = model_type == 'allocated'
        else:
            class_node.model = models['allocated']

        if peek_token() == ':':
            consume()
            class_node.inherits = module.find_def(next_token())
        
        if read_block:
            block_tokens = [next_token()]
            assert block_tokens[0] == '[', f"Expected '[', got {token.value} at line {token.line_num}"
            level = 1
            while level > 0:
                token = next_token()
                if token == '[': level += 1
                if token == ']': level -= 1
                block_tokens.append(token)
            class_node.block_tokens = block_tokens
        
        return class_node

    # we want type-var[init] to allow for construction with variable types (same as python)
    # in this way we support the meta types easier

    # how do i use this state, push a new one, parse statements and then pop. i suppose i know
    def translate(class_def:ENode, method:EMethod):
        print('translating method %s::%s\n' % (class_def.name, method.name))
        assert len(method.body) > 0
        # since translate is inside the read_module() method we must have separate token context here
        push_token_state(method.body)
        reset_member_depth()
        # when we parse our class, however its runtime we dont need to call the method; its on the instance
        for name, a_members in class_def.members.items():
            for member in a_members:
                push_member(member) # if context name not given, we will perform memory management (for args in our case)
        push_member_depth()
        for name, member in method.args.items():
            push_member(member)
        # we need to push the args in, as well as all class members (prior, so args are accessed first)
        enode = parse_statements()
        assert isinstance(enode, EStatements), 'expected statements'
        
        # self return is automatic -- however it might be nice to only do this when the type is not expressed by the user
        if (not method.type_expressed and method.type 
            and method.type.definition == class_def
            and isinstance(enode.value[len(enode.value) - 1], EMethodReturn)):
            enode.value.append(EMethodReturn(type=method.type, value=lookup_member('self')))

        pop_token_state()
        print('printing enodes for method %s' % (method.name))
        print_enodes(enode)
        return enode

    # top level module parsing
    while index < len(tokens):
        token = next_token()
        if token == 'class':
            index -= 1  # Step back to let parse_class handle 'class'
            class_node = parse_class()
            assert not class_node.name in module.defs, '%s duplicate definition' % (class_node.name)
            module.defs[class_node.name] = class_node
            continue

        elif token == 'import':
            index -= 1  # Step back to let parse_class handle 'class'
            import_node = parse_import()
            assert not import_node.name in module.defs, '%s duplicate definition' % (import_node.name)
            module.defs[import_node.name] = import_node
            import_node.process(module)
            continue

    #module.finish_clang_imports()
        
    # finish parsing classes once all identities are known
    for name, enode in module.defs.items():
        if isinstance(enode, EClass): finish_class(enode)

    # now we graph the methods
    for name, enode in module.defs.items():
        if isinstance(enode, EClass):
            for name, a_members in enode.members.items():
                for member in a_members:
                    if isinstance(member, EMethod):
                        member.code = translate(class_def=enode, method=member)
                        print('method: %s' % name)
                        print_enodes(member.code)
        elif isinstance(enode, EImport):
            pass # 

def change_ext(f: str, ext_to: str) -> str:
    base, _ = os.path.splitext(f)
    return f"{base}.{ext_to.lstrip('.')}"

def format_identifier(text):
    u = text.upper()
    f = u.replace(' ', '_').replace(',', '_').replace('-', '_')
    while '__' in f:
        f = f.replace('__', '_')
    return f

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
                            arg_types += ', %s' % str(a.type.definition.name)
                            # will need a suffix on extra member functions 
                            # (those defined after the first will get _2, _3, _4)
                        if member_name == 'cast':
                            file.write('\ti_cast(X,Y,Z, %s)\\\n' % (
                                member.type.definition.name))
                        else:
                            file.write('\t%s_method(X,Y,Z, %s, %s%s)\\\n' % (
                                's' if member.static else 'i',
                                member.type.definition.name, member.name, arg_types))
            file.write('\n')
            if cl.inherits:
                file.write('declare_mod(%s, %s)\n' % (cl.name, cl.inherits))
            else:
                file.write('declare_class(%s)\n' % (cl.name))
    file.write('#endif\n')

def replace_nodes(enode, replacement_func):
    # Check if the current node should be replaced
    replacement = replacement_func(enode)
    if replacement is not None:
        return replacement

    return enode

def file_text(f):
    file = open(c_module_file, 'r')

# all of the basic types come directly from A-type implementation
# string, array, map, hash-map etc

# example usage:
# i need to simply emit these into class-defs in python
# no use exposing model, the user may simply use one of the primitives
input = """
import A    ['https://github.com/ar-visions/A']
import wgpu ['https://github.com/ar-visions/dawn@2e9297c45f48df8be17b4f3d2595063504dac16c']

import <stdlib, stdint, stdio>

class test1 [
    int val: 2
    int init2[] [ return 1 ]
    init [
        print['testing convert: int %i -> string %s'] # % [val, [string]val]]
    ]
    cast i64 [ return 1 ]
]

class app [
    int arg: 0
    string another
    intern string hidden: 'dragon'
    int32_t arg1: 0 # C types are converted to our own primitives

    # called by run
    num something[ num x, f64 mag ] [ return x * 1 * mag ]

    # app entrance method; one could override init as well
    num run[] [
        u8 i: 2 * arg
        u16 a: 2
        u32 n: 1
        num s: something[ 1, 2.0 ]

        while [1] [
            num s: something[ 1, 2.0 ]
        ]
        u32 n2: 2
        return n + something[ a * i, 2 ]
    ]
]
"""

# disable any 'app' or 'main' class found in module if its not top-level
# compilation is separate for different end-points

# silver44 (0.4.4) is python, but 1.0.0 will be C (lol maybe)

build_root    = os.getcwd()
first         = sys.argv[1]
module        = EModule(path=Path(first)) # path is relative from our build root (cwd)
source_tokens = read_plain_tokens(input, module=module)
app_key       = Tokens('app', module)
app           = module.defs[app_key] if 'app' in module.defs else None

if app: app.print()

module.emit(app_def=app)