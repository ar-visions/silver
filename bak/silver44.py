from    dataclasses import dataclass, field, fields, is_dataclass
from    typing      import Union, Type, List, OrderedDict, Tuple, Any
from    pathlib     import Path
import  numpy as np
import  os, subprocess, platform
import  argparse
from    clang.cindex import Index, Config, CursorKind, TypeKind
from    llvmlite     import ir, binding
import  ctypes


# Initialize LLVM binding
binding.initialize()
binding.initialize_native_target()
binding.initialize_native_asmprinter()

# Create an LLVM module and a function definition
module    = ir.Module(name='module')
func_type = ir.FunctionType(ir.IntType(32), [])
main_func = ir.Function(module, func_type, name='main')
block     = main_func.append_basic_block(name='entry')
builder   = ir.IRBuilder(block)

# Return the integer 42 from the main function
builder.ret(ir.Constant(ir.IntType(32), 42))

# run graph at station
def run():
    # Create a target machine and a JIT execution engine
    target   = binding.Target.from_default_triple()
    machine  = target.create_target_machine()
    engine   = binding.create_mcjit_compiler(binding.parse_assembly(str(module)), machine)
    engine.finalize_object()
    engine.run_static_constructors()

    a_main   = engine.get_function_address('main')      # get main address
    f_main   = ctypes.CFUNCTYPE(ctypes.c_int)(a_main) # cast the function pointer to a callable function
    result   = f_main()                                   # call main
    print('Executed main function, returned:', result)
    return result

# llvm emits .ll file
def cerealize(file='a.ll'):
    with open(file, 'w') as f:
        f.write(str(module))
    return True

# Call either function
run()  # To JIT and run the function
# cerealize()  # Uncomment to emit the LLVM IR to a .ll file


if(2 + 5 > 1): # dont darken the code below
    exit(0)

# Initialize the LLVM JIT engine (necessary to access version info)
binding.initialize()
binding.initialize_native_target()
binding.initialize_native_asmprinter()

# Print the LLVM version that llvmlite is using
print(binding.llvm_version_info)


Config.set_library_path('/usr/lib/llvm-17/lib')

build_root = ''
is_debug = False
verbose  = False

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
    '=':   'assign',
    '+=':  'assign_add',
    '-=':  'assign_sub',
    '*=':  'assign_mul',
    '/=':  'assign_div',
    '|=':  'assign_or',
    '&=':  'assign_and',
    '^=':  'assign_xor',
    '>>=': 'assign_right',
    '<<=': 'assign_left',
    '==':  'compare_equal',
    '!=':  'compare_not',
    '%=':  'mod_assign',
    'is':       'is',      # keyword
    'inherits': 'inherits' # if its a keyword, we do not have ability for user to override it
}

keywords = [ 'class',  'proto',    'struct', 'import', 'typeof', 'schema', 'is', 'inherits',
             'init',   'destruct', 'ref',    'const',  'volatile', # 'enum' <- EIdent parsing issue with enum (breaks Clang import)
             'return', 'asm',      'if',     'switch', # no fall-through on switch, and will require [ blocks for more than one expression ]
             'while',  'for',      'do',     'signed', 'unsigned', 'cast' ]

# EIdent are defined as both type identifiers and member identifiers, combined these keywords a decoration map
consumables = [ 'ref',  'schema', 'enum', 'class', 'union', 'proto', 'struct', 'const', 'volatile', 'signed', 'unsigned' ]

assign   = [ ':',   '=' , '+=',  '-=', '*=',  '/=', '|=',
             '&=', '^=', '>>=', '<<=', '%=' ]

compare  = [ '==', '!=' ]

# global to track next available ID (used in ENode matching)
_next_id        = 0

# default paths for Clang and gcc
_default_paths  = ['/usr/include/', '/usr/local/include']

def first_key_value(ordered:OrderedDict):
    assert len(ordered), 'no items'
    return next(iter(ordered.items()))

def exe_ext():
    system = platform.system().lower()
    if system == 'windows':
        return 'exe'
    else:
        return ''

def shared_ext():
    system = platform.system().lower()
    if   system == 'linux':  return 'lib', 'so'
    elif system == 'darwin': return 'lib', 'dylib'
    elif system == 'win32':  return '',    'dll'
    assert False, 'unknown platform'

def static_ext():
    if sys.platform.startswith('linux'): return 'lib', 'a'
    elif sys.platform == 'darwin':       return 'lib', 'a'
    elif sys.platform == 'win32':        return '',    'lib'
    assert False, 'unknown platform'

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
    
    def __bool__(self):
        if hasattr(self, 'name'): return bool(self.name)
        if hasattr(self, 'type'): return self.type != None
        return False

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

def is_numeric(s):
    s = str(s)
    if len(s) > 0:  # Ensure the string is not empty
        return s[0].isnumeric() or s[0] == '-'
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
#models['real-128']    = EModel(name='real_128',    size=16, integral=0, realistic=1, type=np.float128)
models['none']        = EModel(name='none',        size=0, integral=0, realistic=1, type=Void)
models['cstr']        = EModel(name='cstr',        size=8, integral=0, realistic=0, type=np.uint64)
models['symbol']      = EModel(name='symbol',      size=8, integral=0, realistic=0, type=np.uint64)
models['handle']      = EModel(name='handle',      size=8, integral=0, realistic=0, type=np.uint64)

test1 = 0

@dataclass
class EMeta(ENode):
    args:       OrderedDict      = None

# tokens can be decorated with any data
@dataclass
class EIdent(ENode):
    decorators: OrderedDict      = field(default_factory=OrderedDict)
    list:       List['Token']    = field(default_factory=list)
    ident:      str              = None # all combinations of tokens are going to yield a reduced name that we use for matching.  we remove decorators from this ident, but may not for certain ones.  we can change type matching with that
    initial:    str              = None
    module:    'EModule'         = None
    is_fp:      bool             = False
    kind:       object = None
    base:      'EMember'         = None
    meta_types: OrderedDict      = field(default_factory=OrderedDict)
    args:       OrderedDict      = None
    members:    List['EMember']  = None
    ref_keyword: bool            = False
    conforms:   'EMember'        = None
    meta_member:'EMetaMember'    = None
    #
    def __init__(
            self,
            tokens:     Union['EModule', 'EIdent', str, 'EMember', 'Token', List['Token']] = None,
            kind:       object = None,
            meta_types: List['EIdent']   = None, # we may provide meta in the tokens, in which case it replaces it; or, we can provide meta this way which resolves differently; in object format, it may encounter tokens or strings
            decorators: OrderedDict      = OrderedDict(),
            base:      'EMember'         = None,
            conforms:  'EMember'         = None,
            module:    'EModule'         = None):
        
        self.module = module
        self.conforms = conforms
        self.meta_types = meta_types if meta_types else OrderedDict()
        
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
            if tokens == 'void*':
                self
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
            if self.initial == 'union anonymous':
                self
            read_tokens     = module.read_tokens(tokens)
            if index_of(tokens, '*)') >= 0:
                tokens
            ident           = EIdent.parse(module, read_tokens, 0)
            self.decorators = ident.decorators.copy() if ident.decorators else OrderedDict()
            self.list       = ident.list.copy() if ident.list else []
        else:
            self.list = []
        
        if kind == 'pointer' or kind == TypeKind.POINTER:
            assert self['ref'], 'Clang gave TypeKind.POINTER without describing'

        self.updated()

    def get_target(self):
        if self.meta_member:
            return self.module.lookup_stack_member('self')
        if self.members:
            count = len(self.members)
            if count > 1:
                if isinstance(self.members[count - 2], EMember):
                    return self.members[count - 2]
            else:
                return self.members[0]
        return None

    def member_lookup(self, etype:'EIdent', name:str) -> 'EMember':
        while True:
            cl = etype.get_def()
            if name in cl.members:
                assert isinstance(cl.members[name], list), 'expected list instance holding member'
                return cl.members[name][0]
            if not cl.inherits:
                break
            cl = cl.inherits
        return None
    
    def ref_type(self, count = 1):
        res = EIdent(self, module=self.module)
        if not res['ref']:
            assert count > 0, 'invalid dereference'
            res['ref']  = count
        else:
            res['ref'] += count
        return res

    def peek(module):
        module.push_token_state(module.tokens, module.index)
        ident = EIdent.parse(module, module.tokens, offset=module.index)
        module.pop_token_state()
        return ident
    
    # add ability to parse member from Tokens
    # we want:
    # type obj-type = if rand.coin [typeof SomeClass] else [typeof SomeClass2]

    # this will be looked up and called at run-time:
    # the run-time must implement and use convert_args the same

    # obj-type.method-name(args)
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
        proceed    = True
        ref        = 0
        silver_ref = 0
        f          = module.peek_token()
        c_parse    = True
        opaque_compatible = False
        parse_fn   = True

        if f == 'ref': # should still flag the ident with the decoration
            silver_ref = 1
            ref    += 1
            proceed = True
            module.consume(f)
            f       = module.peek_token()
        else:
            ff = str(f)
            if ff in keywords and not ff in ['unsigned', 'signed', 'volatile', 'const', 'struct', 'class', 'union']:
                return result
            opaque_compatible = ff in ['class', 'struct', 'union']
        
        members = module.lookup_all_stack_members(f) # this isnt right, we're making two lists without knowing what separates them
        if members:
            if isinstance(members[0], EMetaMember):
                result.meta_member = members[0]
                members = members[1:]
                member = result.meta_member
            else:
                member = members[0]
            last = member
            module.consume(f)
            len_dec = len(result.decorators)
            assert len_dec == 0 or (result['ref'] and len_dec == 1), 'unexpected decorators'
            result.list.append(member.name)
            while module.peek_token() == '.':
                module.consume('.')
                n = module.next_token()
                assert is_alpha(n), 'expected alpha-name identity after .'
                key = str(n)
                assert result.meta_member or (last.type and key in last.type.get_def().members), 'member "%s" not found in %s' % (key, last.name)
                if result.meta_member:
                    last = key
                else:
                    last = last.type.get_def().members[key][0]
                members.append(last) # we can merge these two
                result.list.append(n)
            c_parse  = False
            parse_fn = False
            proceed  = False
            result.members = members # this is not enough context obviously; we need the entire member chain
        
        # check for definition [ why not after decorators? ]
        if str(f) in module.defs:
            if str(f) == 'void':
                module
            mdef = module.defs[f]
            is_method = isinstance(mdef, EMethod)
            if not is_method and mdef.meta_types and module.peek_token(1) == '::':
                module.consume(f)
                proceed = False
                c_parse = False # redo decorator logic to be a bit more orderly, int unsigned is not a type we want to bind
                result.list.append(f)
                module.consume('::')
                meta_index = 0
                meta_keys = list(mdef.meta_types.keys())
                while True:
                    id = EIdent.parse(module)
                    if id:
                        assert meta_index < len(meta_keys), 'too many meta args specified'
                        result.meta_types[meta_keys[meta_index]] = id
                        meta_index += 1
                    if module.peek_token() == '::':
                        module.consume('::')
                        continue
                    else:
                        assert meta_index == len(meta_keys), 'not enough meta args specified'
                        break
            elif is_method:
                module.consume(f)
                proceed = False
                c_parse = False # redo decorator logic to be a bit more orderly, int unsigned is not a type we want to bind
                result.list.append(f)


        # consume tokens into decorations map
        consume_count = 0
        while proceed:
            proceed = False
            global consumables
            offset = 0
            for con in consumables:
                n = module.peek_token(offset)
                if n in ['(', '[', ']']: break
                if n == con:
                    consume_count += 1
                    offset += 1
                    result[con] = 1
                    proceed = True
                    if module.eof():
                        if push_state:
                            module.pop_token_state()
                        return result
            module.consume(offset)
        
        # consume_count should be 0 for members; they cannot have those keywords (except ref, which was offset above)
        # now we move onto read C99 type keywords and pointers
        iter = 0
        while c_parse:
            iter += 1
            if module.eof(): break
            if module.peek_token() == ')': break
            if module.peek_token() == '(': break
            t = module.peek_token()
            is_extend = t.value in ['long', 'short', 'unsigned', 'signed']
            is_ext = t.value in ['unsigned', 'signed']
            if not is_extend and not is_alpha(t.value):
                if t == ']': break
                if t == '[': break
                if is_numeric(t.value):
                    break
                #assert False, 'debug'
                break
            module.consume(t)
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
                break
            else:
                if not opaque_compatible or iter > 1:
                    break
            if module.eof():
                assert require_next, 'expect type-identifier after ::'
                break
            p = module.next_token()
            is_open = p == '['
            if   is_open and silver_ref:
                break # we want to return ref TheType; the caller will perform heuristics (for ref Type ----> [ value ][ index ])
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
        if parse_fn and not module.eof():
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
                result.args = module.parse_defined_args(is_C99=True)
                module.consume(')')
                result['ref'] = 1

        if push_state:
            module.pop_token_state()
        
        result.ref_keyword = silver_ref
        return result

    def emit(self, ctx:'EContext'): # ctx tells is what state we are emitting in; when are are declaring a variable for instance with meta, its always object
        if ctx.top_state() == 'runtime-type': # when we want to obtain what the type is, thats a lookup on the type::args_[0-7]
            if self.meta_member:
                return 'meta_t(self, %i)' % self.meta_member.index
            else:
                return 'typeof(%s)' % self.get_name()
        else:
            if self.meta_member:
                return 'object'
            else:
                return self.get_name()

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
        if self.meta_member:
            ident_name = str(self.list[-1])
        else:
            ident_name = ' '.join(str(token) for token in self.list) # + ('*' * ref)
        #ident_size = ''
        #if 'dim' in self.decorators:
        #    for sz_int in self.decorators['dim']:
        #        ident_size += '[%s]' % sz_int
        self.ident = ident_name # + ident_size
        if self.meta_types:
            assert self.get_def(), 'no definition for type: %s' % self.ident
            self.module.register_meta_user(self)


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
        if isinstance(other, str):
            return self.ident == other or self.initial == other
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
        member_def = self.module.lookup_stack_member(self) # fix this to look at the entire series
        if member_def:
            return member_def
        if not self.ident in self.module.defs:
            return None
        mdef = self.module.defs[self.ident]
        return mdef
    
    def get_c99_members(self, access):
        res = access if access else ''
        for m in self.members:
            if res: res += '->'
            if isinstance(m, EMember):
                res += '%s' % m.name
            else:
                assert isinstance(m, str), 'invalid member format'
                res += '%s' % m
        return res

    def get_base(self):
        mdef = self.get_def()
        while hasattr(mdef, 'to'):
            mdef = mdef.to.get_def()
        return mdef

    def get_name(self):
        mdef = self.get_def()
        assert mdef, 'no definition resolved on EIdent'
        if isinstance(mdef, EClass):
            res = mdef.name # should be same as self.ident
            if self.meta_types:
                for symbol, type in self.meta_types.items():
                    if res: res += '_'
                    res += type.get_name()
        else:
            res = mdef.name
        ref = self['ref'] or 0
        ptr = '*' * ref
        if ptr: res += ptr
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

def folder_path(path):
    p = Path(path)
    return p if p.is_dir() else p.parent

@dataclass
class EContext:
    module: 'EModule'
    method: 'EMember'
    states: list = field(default_factory=list)
    raw_primitives: bool = False
    values: OrderedDict = field(default_factory=OrderedDict)
    indent_level:int = 0

    def indent(self):
        return '\t' * self.indent_level
    def increase_indent(self):
        self.indent_level += 1
    def decrease_indent(self):
        if self.indent_level > 0:
            self.indent_level -= 1
    def set_value(self, key, value):
        self.values[key] = value
    def get_value(self, key):
        return self.values[key] if key in self.values else None
    def push(self, state):
        self.states.append(state)
    def pop(self):
        self.states.pop()
    def top_state(self):
        return self.states[len(self.states) - 1] if self.states else ''

@dataclass
class EMember(ENode):
    name:   str
    type:   EIdent   = None
    module:'EModule' = None
    value:  ENode    = None
    parent:'EClass'  = None
    access: str      = None
    imported:bool    = False
    emitted:bool     = False
    members:OrderedDict = field(default_factory=OrderedDict) # lambda would be a member of a EMethod, in one example
    args:OrderedDict[str,'EMember'] = None
    context_args:OrderedDict[str, 'EMember'] = None
    meta_types:OrderedDict    = None
    meta_model:OrderedDict    = None # key = symbol, value = conforms (class or proto)
    static:bool = None
    visibility: str = None
    def emit(self, ctx:EContext):
        is_deref = '*' if self.type.ref_keyword and not ctx.top_state() == 'declare' else ''
        if not self.access or self.access == 'const':
            return '%s%s'     % (is_deref, self.name)
        else:
            return '%s%s->%s' % (is_deref, self.access, self.name)

@dataclass
class EMetaMember(EMember):
    conforms:EIdent = None
    index:EIdent = None
    def emit(self, ctx:EContext):
        return 'meta_t(self, %i)' % self.index 

@dataclass
class EDeclaration(ENode):
    type:  EIdent  = None
    target:EMember = None
    def emit(self, ctx:EContext):
        return '%s %s' % (self.type.emit(ctx), self.target.name)

@dataclass
class EMethod(EMember):
    method_type:str = None
    type_expressed:bool = False
    body:EIdent = None
    statements:'EStatements' = None
    code:ENode = None
    auto:bool = False
    context: OrderedDict[str, EMember] = None # when emitting, this should create a struct definition as well; self is passed in as an arg as our methods are 

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

    def emit_header(self, f):
        name = self.name
        cl = self
        # we will only emit our own types
        if cl.model.name != 'allocated' or cl.emitted: return
        # write class declaration
        f.write('/* class-declaration: %s */\n' % (name))
        f.write('#define %s_schema(X,Y,Z) \\\n' % (name))
        for member_name, a_members in cl.members.items():
            for member in a_members:
                if isinstance(member, EProp):
                    # todo: handle public / private / intern [ even intern must declare its space in the instance less we reserve special space for intern ]
                    base_type = member.type.get_base()
                    f.write('\ti_%s(X,Y,Z, %s, %s)\\\n' % (
                        member.visibility,
                        base_type.name, member.name))
        for name, a_members in cl.members.items():
            for member in a_members:
                if member.visibility != 'intern' and isinstance(member, EMethod):
                    m          = 's' if member.static else 'i'
                    arg_types  = ''
                    first_type = ''
                    mdef       = member.type.get_base()
                    for _, a in member.args.items():
                        arg_type = a.type.get_name()
                        if not first_type: first_type = arg_type
                        arg_types += ', %s' % arg_type
                        # will need a suffix on extra member functions 
                        # (those defined after the first will get _2, _3, _4)
                    if   name == 'cast':  f.write('\ti_cast(     X,Y,Z, %s)\\\n'        % (mdef.name))
                    elif name == 'ctr':   f.write('\ti_construct(X,Y,Z%s)\\\n'          % (arg_types))
                    elif name == 'index': f.write('\ti_index(    X,Y,Z, %s%s)\\\n'      % (mdef.name, arg_types))
                    else:                 f.write('\t%s_method(   X,Y,Z, %s, %s%s)\\\n' % (m, mdef.name, member.name, arg_types))
        f.write('\n')
        if cl.inherits:
            f.write('declare_mod(%s, %s)\n\n' % (cl.name, cl.inherits.name))
        else:
            f.write('declare_class(%s)\n\n' % (cl.name))

    def output_methods(self, file, cl, with_body):
        for member_name, a_members in cl.members.items():
            for member in a_members:
                if isinstance(member, EMethod):
                    args = ', ' if not member.static and len(member.args) else ''
                    for arg_name, a in member.args.items():
                        if args and args != ', ': args += ', '
                        args += '%s%s %s' % (
                            a.type.get_def().name, '*' * a.type.ref(), a.name)
                    
                    full_name = '%s_%s' % (cl.name, member.name)
                    file.write('%s %s(%s%s)%s\n' % (
                        member.type.get_def().name,
                        full_name,
                        '' if member.static else cl.name + ' self',
                        args,
                        ';\n' if not with_body else ' {'))
                    if with_body:
                        file.write(member.code.emit(EContext(module=self, method=member))) # method code should first contain references to the args
                        file.write('}\n\n')
    
    def emit_source_decl(self, file):
        cl = self
        if cl.model.name != 'allocated' or cl.emitted: return
        if cl.visibility == 'intern':
            self.emit_header(file)
        self.output_methods(file, cl, False)  # output all methods with body
        
    def emit_source(self, file):
        cl = self
        if cl.model.name != 'allocated' or cl.emitted: return
        self.output_methods(file, cl, True)  # output all methods with body
        if cl.inherits:
            file.write('define_mod(%s, %s)\n\n' % (cl.name, cl.inherits.name))
        else:
            file.write('define_class(%s)\n\n' % (cl.name))

# we have no map yet, so we need that.  its index takes in an A object
# how do we get array imported?

@dataclass
class EStruct(EMember):
    def __repr__(self): return 'EStruct(%s)' % self.name
    def __post_init__(self):
        self.model = models['struct']
    def emit_header(self, f, is_source = False):
        if self.imported: return
        is_intern = self.visibility == 'intern'
        if not is_source and is_intern: return
        f.write('typedef struct {\n' % self.name)
        valid = False
        for name, a_members in self.members.items():
            for member in a_members:
                assert isinstance(member, EProp)
                type = member.type.get_name()
                name = member.name
                f.write('\t%s %s;\n' % (type, name))
                valid = True
        assert valid, 'struct %s must contain members' % self.name
        f.write('} %s;\n' % self.name)
    def emit_source_decl(self, f): return
    def emit_source(self, f):
        if self.visibility == 'public':
            self.emit_header(f, True)

@dataclass
class EUnion(EMember):
    def __repr__(self): return 'EUnion(%s)' % self.name
    def __post_init__(self):
        self.model = models['struct']
    def emit_header(self, f, is_source = False):
        if self.imported: return
        assert False, 'union not supported'
    def emit_source_decl(self, f):
        pass
    def emit_source(self, f):
        if self.imported: return
        assert False, 'union not supported'

@dataclass
class EEnum(EMember):
    def __repr__(self): return 'EEnum(%s)' % self.name
    def __post_init__(self):
        self.model = models['signed-32']
    def emit_header(self, f):
        name = self.name
        if self.imported: return
        if self.visibility != 'public': return
        f.write('\n/* enum-declaration: %s */\n' % (name))
        f.write('#define %s_meta(X,Y,Z) \\\n' % (name))
        for _, a_members in self.members.items():
            assert len(a_members) == 1, 'invalid enumeration'
            for member in a_members:
                f.write('\tenum_value(X,Y, %s)\\\n' % (member.name))
                break
        f.write('\ndeclare_enum(%s)\n' % (name))
    def emit_source_decl(self, f):
        pass
    def emit_source(self, f):
        f.write('define_enum(%s)\n' % (self.name))

@dataclass
class EAlias(EMember):
    to:EIdent=None
    def __repr__(self): return 'EAlias(%s -> %s)' % (self.name, self.to.get_name()) 
    def emit_header(self, f): return
    def emit_source_decl(self, f): return
    def emit_source(self, f): return

@dataclass
class BuildState:
    none=0
    built=1

def create_symlink(target, link_name):
    if os.path.exists(link_name):
        os.remove(link_name)
    os.symlink(target, link_name)
    

@dataclass
class EImport(ENode): # we may want a node for 'EModuleNode' that contains name, visibility and imported
    name:       str
    source:     str
    includes:   List[str] = None
    cfiles:     List[str] = field(default_factory=list)
    links:      List[str] = field(default_factory=list)
    imported:bool = False
    build_args: List[str] = None
    import_type = 0
    library_exports:list = None
    visibility:str = 'intern'
    none=0
    source=1
    library=2
    # header=3 # not likely using; .h's are selected from source
    project=4
    main_symbol: str = None

    def emit_header(self, f):
        for inc in self.includes:
            h_file = inc
            if not h_file.endswith('.h'): h_file += '.h'
            f.write('#include <%s>\n' % (h_file))

    def emit_source(self, f): return

    def emit_source_decl(self, f): return

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
                    pre, ext = shared_ext()
                    lib = os.path.join(i, 'lib', f'{pre}{name}.{ext}')
                    if not os.path.exists(lib):
                        pre, ext = static_ext()
                        lib = os.path.join(i, 'lib', f'{pre}{name}.{ext}')
                    assert os.path.exists(lib)
                    sym = os.path.join(i, f'{pre}{name}.{ext}')
                    create_symlink(lib, sym)

                built_token = open('silver-token', 'w') # im a silver token, i say we seemed to have built before -- if this is not here, we perform silver-init.sh
                built_token.write('')
                built_token.close()
        
        return BuildState.built

    def build_source(self):
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
            
            obj        = '%s.o' % cfile
            obj_path   = Path(obj)
            log_header = 'import: %s source: %s' % (self.name, cfile)
            print('%s > %s\n' % (cwd, compile))
            assert system(compile) == 0, '%s: compilation failed'    % (log_header)
            assert obj_path.exists(),    '%s: object file not found' % (log_header)

            if contains_main():
                self.main_symbol = '%s_main' % obj_path.stem()
                assert system('objcopy --redefine-sym main=%s %s' % (self.main_symbol, obj)) == 0, '%s: could not replace main symbol' % log_header

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
                self.build_source()
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

# constructors require an argument (alloc + ctr + init), otherwise we use a new (alloc + init)
@dataclass
class EConstruct(ENode):
    type: EIdent
    method: EMethod
    args:List[ENode] # when selected, these should all be filled to line up with the definition
    meta_member:'EMetaMember' = None
    def emit(self, ctx:EContext):
        if self.meta_member:
            if not self.args:
                return 'A_new(%s)' % self.meta_member.emit(ctx)
            else:
                args = ''
                n_args = 0
                for a in self.args:
                    arg_emit = a.emit(ctx)
                    args    += ', '
                    args    += arg_emit
                    n_args  += 1
                return 'A_construct(%s, %i%s)' % (self.meta_member.emit(ctx), n_args, args) # when we have explicit arg counts we want machines to only call these
        
        type_name = self.type.ident
        if self.args:
            assert self.method, 'method not set in EConstruct, and type is fixed'
            args = ''
            f = self.args[0]
            base0 = f.type.get_base()
            assert base0, 'base type not resolving'
            for a in self.args:
                arg_emit = a.emit(ctx)
                args += ', '
                args += arg_emit
            return 'ctr(%s, %s%s)' % (type_name, base0.name, args)
        else:
            return 'new(%s)' % (type_name)

@dataclass
class EExplicitCast(ENode):
    type: EIdent
    value: ENode
    def emit(self, ctx:EContext):
        return '(%s)(%s)' % (self.type.emit(ctx), self.value.emit(ctx))
    
@dataclass
class EPrimitive(ENode):
    type: EIdent
    value: ENode
    def emit(self, ctx:EContext):
        if ctx.raw_primitives:
            return self.value.emit(ctx)
        else:
            return 'A_%s(%s)' % (self.value.type.emit(ctx), self.value.emit(ctx))

@dataclass
class EProp(EMember):
    is_prop:bool = True
    def __post_init__(self):
        super().__post_init__()
        if self.id == 4501:
            self


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
class ERef(ENode):
    type:      'EIdent' = None
    value:      ENode  = None
    def emit(self, ctx:'EContext'):
        type_name = self.type.get_name()
        assert self.type.ref() > 0, 'invalid type for ref'
        if self.value:
            value = self.value.emit(ctx)
            return '(%s)&%s'  % (type_name, value)
        else:
            return '(%s)null' % type_name


@dataclass
class ERefCast(ENode):
    type:      'EIdent' = None
    value:      ENode  = None
    index:      ENode  = None
    def emit(self, ctx:'EContext'):
        type_name = self.type.get_name()
        assert self.type.ref() > 0, 'invalid type for ref'
        if isinstance(self.index, ELiteralInt) and self.index.value != 0: # based on if its a C99 type or not, we will add another *
            return '(%s)(&(*(%s))%s[%d])' % (type_name, self.value.name, self.index.value)
        elif self.value:
            return '(%s)%s'      % (type_name, self.value.name)
        else:
            return '(%s)null'    % type_name

@dataclass
class EIndex(ENode):
    type:EIdent
    target:ENode
    value:ENode
    def emit(self, ctx:EContext):
        type_str = self.type.emit(ctx)
        value    = self.value.emit(ctx)
        target   = self.target.emit(ctx)
        return '(%s)%s[%s]' % (type_str, target, value)

@dataclass
class EAssign(ENode):
    type:EIdent
    target:ENode
    value:ENode
    index:ENode # if this is here, its an indexed assignment for ref Type [ target ] [ index ] : value
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

        type_str = self.type.emit(ctx)
        # we need to place this in if we are converting manually
        ctx.push('assign')
        if self.declare:
            ctx.push('declare') # these would be good to reduce out but EMember is doing the work here, and for references there are rules it has no context on
        value  = self.value.emit(ctx)
        target = self.target.emit(ctx)
        ctx.pop()
        if isinstance(self.index, ENode):
            assert not self.declare, 'cannot declare with index set' # this should be a parsing error
            return '((%s)%s)[%s] %s %s' % (type_str, target, self.index.emit(ctx), op, value)
        else:
            if self.declare:
                assert op in [':', '='], ': or = required for assignment on new members'
                return '%s %s = %s' % (type_str, target, value)
            else:
                return '%s %s %s'   % (target, op, value)

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


assign_map = {
    ':':   EAssign,
    '=':   EAssign,
    '+=':  EAssignAdd,
    '-=':  EAssignSub,
    '*=':  EAssignMul,
    '/=':  EAssignDiv,
    '|=':  EAssignOr,
    '&=':  EAssignAnd,
    '^=':  EAssignXor,
    '>>=': EAssignShiftR,
    '<<=': EAssignShiftL,
    '%=':  EAssignMod
}

@dataclass
class EIf(ENode):
    type: EIdent # None; can be the last statement in body?
    condition:ENode
    body:'EStatements'
    else_body:ENode
    def emit(self, ctx:EContext):
        e  = ''
        e += 'if (%s) {\n' % self.condition.emit(ctx)
        e += self.body.emit(ctx) # Statements increases the indent, and we already outputted our indent level above?
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
    args:object = None
    # todo:
    def emit(self, n:EContext): return '"%s"' % self.value[1:len(self.value) - 1]

@dataclass
class ELiteralBool(ENode):
    type: EIdent
    value:bool
    def emit(self,  n:EContext):  return 'true' if self.value else 'false'

@dataclass
class ESubProc(ENode):
    type:EIdent
    target:EMember
    method:EMember
    context_type:EIdent
    context_args:List[ENode]
    def emit(self, ctx:EContext):
        # A_member(meta_t(self, %i), A_TYPE_IMETHOD | A_TYPE_SMETHOD, "%s");
        args = ''
        context_keys = list(self.method.context_args.keys())
        index = 0
        #if not self.method.static:
        #    args += 'self' # if the target is external to the caller we will need to get that object,  sub other-obj.public-lambda[ 1 ]
        # we need to generalize this one to work with meta and non-meta cases
        # the sub does not require conversion, however we must perform the right casting with primitives, only calling A_convert when its of an object
        for a in self.context_args:
            if args: args += ', '
            str_e     = a.emit(ctx)
            edef      = a.type.get_base()
            ctx_arg   = self.method.context_args[context_keys[index]]
            type_name = ctx_arg.type.get_name()
            convert   = a.type.ident != type_name
            if convert:
                if edef.model != 'allocated':
                    str_e = '(%s)%s' % (ctx_arg.type.get_name(), str_e)
                else:
                    str_e = 'A_convert(typeof(%s), %s)' % (ctx_arg.type.get_name(), str_e)
            args     += str_e
            index    += 1

        target = self.target.emit(ctx)
        context_keys = list(self.method.context_args.keys())
        first_arg = self.method.context_args[context_keys[0]].type
        return 'A_lambda(%s, A_member(isa(%s), A_TYPE_IMETHOD | A_TYPE_SMETHOD, "%s"), ctr(%s, %s, %s))' % (
            target, target, self.method.name, self.context_type.get_name(), first_arg.get_name(), args)

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
            'EXor':      '^',
            'ECompareEquals': '==',
            'ECompareNotEquals': '!=',
            'EIs':       'is',
            'EInherits': 'inherits' # should be used for evaling the order, consolidating the parse_* methods into 1
        }                           # will not have to pair these as we do, its just calling one after the other
        assert t in m
        self.op = m[t]
        return super().__post_init__()
    def emit(self, ctx:EContext): return self.left.emit(ctx) + ' ' + self.op + ' ' + self.right.emit(ctx)


@dataclass
class ECompareEquals(EOperator):
    pass


@dataclass
class ECompareNotEquals(EOperator):
    pass


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
class EIs(EOperator):
    def template(self): return '%s == %s'
    def emit(self, ctx:EContext):
        # self.left must be a EMember
        # parse primary will be called where we can look for a class + 'is', we
        # would return ERuntimeType
        assert isinstance(self.left,  EIdent), 'expected ident (left operand)'
        assert isinstance(self.right, EIdent), 'expected ident (right operand)'
        ctx.push('runtime-type') # should support all forms of EIdent state in its emit
        l_type_id = self.left.emit(ctx)
        r_type_id = self.right.emit(ctx)
        ctx.pop()
        return self.template() % (l_type_id, r_type_id)

@dataclass
class EInherits(EIs): # EInherits inherits from EIs, just so we can have a different C99 template
    def template(self): return 'inherits(%s, %s)'

@dataclass
class ERuntimeType(ENode):
    type:EIdent
    # this wont work with meta type, we need more because those are unique type ids
    # i just wonder how we would name them in A, simply array_int ? for array::int ?
    def emit(self, ctx:EContext):
        return 'typeof(%s)' % self.type.c99_name()

def append_args(s_args, args:List[ENode], method_key:str, convert:bool, ctx:EContext):
    for i in range(len(args)):
        if s_args: s_args += ', '
        arg_type = args[i].type
        str_e    = args[i].emit(ctx)
        if convert:
            edef = arg_type.get_base()
            if (edef.model != 'allocated'):
                str_e = 'A_%s(%s)' % (edef.name, str_e)
            str_e = 'A_convert(%s->args.meta_%i, %s)' % (method_key, i, str_e)
        s_args += str_e
    return s_args

@dataclass
class EMethodCall(ENode):
    type:EIdent
    target:ENode
    method:ENode
    args:List[ENode]
    arg_temp_members:List[EMember] = None

    def emit(self, ctx:EContext):
        s_args   = ''
        if isinstance(self.method, EMethod):
            arg_keys = list(self.method.args.keys())
            arg_len  = len(arg_keys)
        else:
            arg_len = 0
        
        is_meta = isinstance(self.method, EIdent)
        assert not is_meta or self.method.meta_member, 'invalid EIdent given'
        assert is_meta or arg_len == len(self.args) # this is already done above
        is_arr = False
        method_id = self.method

        if is_meta:
            if self.target.meta_member:
                s_args += 'array_of_objects(self'
                is_arr  = True
            else:
                assert False, 'implement 1'

        if not self.target:
            return '%s(%s)' % (self.method.name, s_args)
        else:
            # intern should call a bit faster; we will need to perform some polymorphic lookup here
            if is_meta:
                method_name = method_id.members[-1]
                method_key  = method_id.ident # we cannot lookup a definition, we may only use strings
                var_name    = ctx.get_value(method_key) # should use C99 concat'd 
                if not var_name:
                    get_method = ctx.indent() + 'type_member_t* %s = A_member(meta_t(self, %i), A_TYPE_IMETHOD | A_TYPE_SMETHOD, "%s");' % (
                        method_key,  method_id.meta_member.index, method_name)
                    ctx.set_value(method_key, get_method) 

                # convert args by wrapping their expression in A_convert(AType, A)
                # using pooled release is better for our interpreter and the code it generates is smaller, more readable.
                # its quite simple to manage your own pools in heavy usage areas
                # auto-release pool # <- use this in statements such as a method, or loop
                # pool.release with mod is a reasonable technique, so its 2 calls to tune your overhead/memory usage
                # upon creation it pushes its pool onto the thread local stack
                # on dealloc it does the opposite
                # its probably quite useful to have anonymous classes that exist while code runs
                # auto-release # may just use this, without a need to call a .release, we dont need the handle of it
                # anonymous satisfies thread-local (no args) users and context consumers (with the use of args)
                # passing by value is not quite practical, so struct's should be always by ref

                s_args = append_args(s_args, self.args, method_key, is_arr == True, ctx)
                if is_arr: s_args += ')'

                return 'A_method_call(%s, %s)' % (method_key, s_args) 
            elif self.method.visibility == 'intern':
                s_args = append_args(s_args, self.args, None, False, ctx)
                assert ctx.method.parent == self.method.parent, 'intern method accessed outside type'
                return '%s_%s(%s%s%s)' % (
                    self.method.parent.name, # meta args must be in ctx
                    self.method.name,
                    self.target,
                    ', ' if s_args else '',
                    s_args)
            else:
                s_args = append_args(s_args, self.args, None, False, ctx)
                return 'call(%s, %s%s%s)' % (
                    self.target.emit(ctx),
                    self.method.name,
                    ', ' if s_args else '',
                    s_args)
    

@dataclass
class EMethodReturn(ENode): # v1.0 we will want this to be value:List[ENode]
    type:EIdent
    value:ENode

    def emit(self, ctx:EContext):
        if self.type.ref() == 0 and self.type.get_base().name == 'none':
            return 'return'
        else:
            return 'return %s' % self.value.emit(ctx)
    

@dataclass
class EStatements(ENode):
    type:EIdent # last statement type
    value:List[ENode]

    def emit(self, ctx:EContext):
        res = ''
        ctx.increase_indent()
        for enode in self.value:
            res += ctx.indent()
            code = enode.emit(ctx)
            res += code
            if code[-2:] != '}\n':
                res += ';\n'

        header = ''
        for key, val in ctx.values.items():
            header += '%s\n' % val
        ctx.decrease_indent()
        return header + res

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
    expr_level:     int                         = 0
    # moving the parser inside of EModule
    # move to EModule (all of these methods)
    index:int             = 0
    tokens:List[Token]    = field(default_factory=list)
    token_bank:list       = field(default_factory=list)
    libraries_used:list   = field(default_factory=list)
    compiled_objects:list = field(default_factory=list)
    main_symbols:list     = field(default_factory=list)
    context_state:list    = field(default_factory=list)
    current_def:'EMember' = None
    member_stack:List[OrderedDict[str, List[EMember]]] = field(default_factory=list)
    meta_users:OrderedDict = field(default_factory=OrderedDict)

    def register_meta_user(self, ident:EIdent):
        unique = ident.get_name()
        if not unique in self.meta_users:
            self.meta_users[unique] = ident

    def eof(self):
        return not self.tokens or self.index >= len(self.tokens)
    
    def read_tokens(self, input_string) -> List[Token]: # should we call Token JR ... or Tolken?
        special_chars:str  = '.$,<>()![]/+*:=#'
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
                elif char == '=' and input_string[index + 1] == '=':
                    tokens.append(Token('==', line_num))
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
        f = open(h_module_file, 'w')
        f.write('#ifndef _%s_\n' % ifdef_id)
        f.write('/* generated silver C99 emission for module: %s */\n' % (self.name))
        f.write('#include <A>\n')
        for name, mod in self.parent_modules.items():
            f.write('#include <%s.h>\n' % (name)) # double quotes means the same path as the source
        
        # our public imports; this is so you can expose C types else-where in public methods
        # otherwise we must describe methods as intern, if we want to keep args that use them intern
        # public or private is not determined automatically by design
        # we run the risk of bloating up software without knowing
        # plus, reading code should really tell you. the keyword is public for import
        self.emit_includes(f, 'public')
        #f.close()

        # write the forward declarations
        for name, cl in self.defs.items():
            if isinstance(cl, EClass):
                # we will only emit our own types
                if cl.model.name != 'allocated' or cl.emitted: continue
                # write class declaration
                f.write('\ntypedef struct %s* %s;' % (name, name))    
        f.write('\n')
        
        primitive_types = []

        # emit C99 for 'primitive', registered with A-type
        f.write('\n/* register imported types with A-type */\n')
        imported_defs = ''
        imported_decl = ''
        imported_cdef = ''

        # for now i want to avoid all forms of imported types from libs -- in theory it can be coupled with public imports
        def fn(base_type):
            if base_type.imported and not base_type.emitted:
                primitive_types.append(base_type)
                base_type.emitted = True
                nonlocal imported_defs, imported_decl, imported_cdef
                imported_defs += '\n#define %s_meta(X,Y,Z) imported_meta(X,Y,Z)' % base_type.name
                imported_decl += '\ndeclare_primitive(%s, imported)'             % base_type.name
                imported_cdef += '\ndefine_primitive(%s, imported, 0)'           % base_type.name

        for name, cl in self.defs.items():
            if isinstance(cl, EClass):
                if cl.model.name != 'allocated': continue
                for name, a_members in cl.members.items():
                    for member in a_members:
                        if member.visibility != 'public': continue
                        if isinstance(member, EProp):
                            fn(member.type.get_def())
                        elif member.args:
                            for _, a in member.args.items():
                                fn(a.type.get_def())

        # write the declarations
        for name, cl in self.defs.items():
            if cl.imported: continue
            if cl.visibility != 'public': continue
            if isinstance(cl, EImport): cl.emit_header(f)
        
        f.write(imported_defs)
        f.write(imported_decl)
        f.write('\n')

        # write the declarations
        for name, cl in self.defs.items():
            if cl.imported: continue
            if cl.visibility != 'public': continue
            if not isinstance(cl, EImport): cl.emit_header(f)

        if self.meta_users:
            imported_cdef += '\n'
            for name, ident in self.meta_users.items():
                def_name = ident.get_def().name
                f.write('\ndeclare_meta(%s, %s)' % (def_name, name))
                types = ''
                for symbol, meta_ident in ident.meta_types.items():
                    if types: types += ', '
                    types += meta_ident.get_name()
                imported_cdef += '\ndefine_meta(%s, %s, %s)' % (def_name, name, types)

        f.write('\n#endif\n')
        f.close()
        return imported_cdef
        
    def emit_includes(self, file, visibility):
        for name, im in self.defs.items():
            if isinstance(im, EImport) and len(im.includes) and im.visibility == visibility:
                for inc in im.includes:
                    h_file = inc
                    if not h_file.endswith('.h'): h_file += '.h'
                    file.write('#include <%s>\n' % (h_file))

    def emit(self, app_def):
        os.chdir(build_root)
        c_module_file = '%s.c' % self.name
        if not c_module_file: c_module_file = self.name + '.c'
        assert index_of(c_module_file, '.c') >= 0, 'can only emit .c (C99) source.'
        emit_cfile = '%s/%s' % (os.getcwd(), c_module_file)
        print('emitting C99 to %s' % (emit_cfile))
        file = open(c_module_file, 'w')
        header = change_ext(c_module_file, 'h')
        imported_defs = self.header_emit(self.name, header)
        file.write('#include <%s.h>\n' % (self.name))
        self.emit_includes(file, 'intern')
        file.write('\n')
        self.emit_cfile = emit_cfile
        #file.write(imported_defs + '\n\n')

        for name, cl in self.defs.items():
            if cl.imported: continue
            cl.emit_source_decl(file)
        
        # emit code for classes
        for name, cl in self.defs.items():
            if cl.imported: continue
            cl.emit_source(file)
        
        # output entry if we describe one
        if app_def:
            file.write("""
int main(int argc, char* argv[]) {
    A_finish_types();
    // todo: parse args and set in app class
    %s main = new(%s);
    return (int)call(main, run);
}""" % (app_def.name, app_def.name))
        
        file.close()
        return True

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
        #m.defs['f128']   = EClass(module=m, name='f128', visibility='intern', model=models['real-128'])
        m.defs['int']    = m.defs['i32']
        m.defs['num']    = m.defs['i64']
        m.defs['real']   = m.defs['f64']
        m.defs['none']   = EClass(module=m, name='none', visibility='intern', model=models['none'])
        m.defs['cstr']   = EClass(module=m, name='cstr', visibility='intern', model=models['cstr'])
        m.defs['symbol'] = EClass(module=m, name='symbol', visibility='intern', model=models['symbol'])
        m.defs['handle'] = EClass(module=m, name='handle', visibility='intern', model=models['handle'])
        m.defs['object'] = EClass(module=m, name='object', visibility='intern', model=models['allocated'])
        m.defs['fn']     = EClass(module=m, name='fn',     visibility='intern', model=models['allocated'])

        # should this just go direct into defs?
        #schemas = m.parse_A_schemas('A')

        #for name, edef in schemas.items():
        #    if name == 'A': continue
        #    m.defs[name] = edef
        
        # need to get meta working with this; all permutations of meta must be 'alias' registered (probably should rename?)
        # here we are attempting to declare somem ethods on 'array': todo: we must do all runtime types!
        m.defs['array']  = EClass(module=m, name='array', imported=True, visibility='intern', model=models['allocated'])
        push_args = OrderedDict()
        push_args['element'] = EProp(name='element', type=EIdent('object', module=self), module=self)
        m.defs['array'].members['push'] = [EMethod(name='push', type=EIdent('object', module=self), args=push_args)]
        m.defs['array'].meta_types = OrderedDict()
        I = Token('I')
        m.defs['array'].meta_types[I] = EIdent(I, module=self, conforms=None)

        m.defs['void']   = m.defs['none']
        for name, edef in m.defs.items():
            m.defs[name].emitted = True
            m.defs[name].type = EIdent(m.defs[name], module=self)

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
            #'long double':              'f128',
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

    def build(self):
        # run gcc with emitted source
        os.chdir(build_root)
        has_app = 'app' in self.defs
        link = 'gcc %s -L %s %s %s -o %s/%s' % (
            '' if has_app else '-shared',
            install_dir,
            self.compiled_objects, self.libraries_used,
            build_root, out_stem)
        assert(system(link) == 0)

    def build_dependencies(self):
        assert not self.finished, 'EModule: invalid call to complete'
        global build_root
        for idef in self.defs:
            obj = self.defs[idef]
            if isinstance(obj, EImport):
                im = obj
                if im.import_type == EImport.source:
                    if im.main_symbol: self.main_symbols.append(im.main_symbol)
                    for source in im.source:
                        # these are built as shared library only, or, a header file is included for emitting
                        if source.endswith('.rs') or source.endswith('.h'):
                            continue
                        buf = '%s/%s.o' % (build_root, source)
                        self.compiled_objects.append(buf)
                    break
                elif im.import_type == EImport.library or im.import_type == EImport.project:
                    self.libraries_used += [lib for lib in im.links]
        
    def build(self):
        # set if we are building a shared library or exe
        is_app   = 'app' in self.defs or self.has_main

        # change directory to build-root, set install-path for libs and headers to import
        assert Path(build_root).exists(), 'build root does not exist'
        os.chdir(build_root)
        install  = install_dir()

        # compile module
        cflags   = ['-Wno-incompatible-pointer-types']
        cflags_str = ' '.join(cflags)
        compile  = f'gcc -std=c99 {cflags_str} -I{build_root} -I{install}/include -c {self.name}.c -o {self.name}.o'
        assert(system(compile) == 0)

        # link module with all of the objects compiled (C and Rust) and the import libraries
        link     = f'gcc%s -L%s {self.name}.o %s %s -o %s' % (
            '' if is_app else ' -shared',
            install, # lib path
            ' '.join(self.compiled_objects),  # all.o's
            ' '.join('-l %s' % lib for lib in self.libraries_used), # -l libs
            self.name)
        print('%s > %s\n' % (build_root, link))
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
        if isinstance(clang_def, EIdent):
            edef = clang_def.get_def()
            if not edef and clang_def != 'anonymous':
                edef
            return edef, EIdent(tokens=edef, module=self)
        # there will be cases where 'spelling' wont be there and we have a struct name*
        name = None
        if isinstance(clang_def, tuple):
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
                i32_type = EIdent('i32', module=self)
                enum_def.members[enum_name] = EMember(
                    imported=True,
                    name=enum_name,
                    type=i32_type,  # enums don't have a 'type' in the same sense; you might store their value type
                    parent=enum_def,
                    access=None,
                    value=ELiteralInt(type=i32_type, value=enum_value),
                    visibility='public')
                
        elif kind == 'union':
            self.defs[id] = EUnion(imported=True, name=name, members=OrderedDict(), module=self)
            self.defs[id].type = EIdent(id, kind='union', module=self)

            members = self.defs[id].members
            for m, member_clang_def in clang_def['fields']:
                edef, ftype = self.edef_for(member_clang_def)
                if not edef:
                    continue # on-error-resume-next
                members[m] = EMember(
                    imported=True,
                    name=m,
                    type=ftype,
                    parent=None,
                    access=None,
                    visibility='public')
            
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
                edef, etype = self.edef_for(member_clang_def) # we need the ref data here too
                if not edef:
                    continue # we cannot resolve anonymous members at the moment; it will need to embed them in the various EMembers such as EUnion and EStruct
                # EIdent is more useful than just refs
                assert edef, 'definition not found' 
                members[struct_m] = EMember(
                    imported=True, name=struct_m, type=etype,
                    parent=None, access=None, visibility='intern')
            
        elif kind == 'function':
            rtype_edef, rtype_ident = self.edef_for(clang_def['return_type'])
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
    
    def is_method(self, tokens:EIdent):
        v = self.is_defined(tokens)
        return v if isinstance(v, EMethod) else None
        
    # this wont actually validate the meta ordering which resolve_type subsequently performs
    def is_defined(self, tokens:List[Token]):
        if not tokens:
            return None
        tokens_ident = EIdent(tokens, module=self).ident
        if tokens_ident in self.defs:
            type_def = self.defs[tokens_ident]
            return self.defs[tokens_ident]
        else:
            return None
    
    # lookup access to member. methods must do a bit more with this
    def lookup_stack_member(self, s:str):
        all = self.lookup_all_stack_members(s)
        return all[0] if all else None
    
    def lookup_all_stack_members(self, s:str):
        s = str(s)
        res = []
        for i in range(len(self.member_stack)):
            index = len(self.member_stack) - 1 - i
            map = self.member_stack[index]
            if s in map:
                member_array = map[s]
                #res.insert(0, member_array)
                res += member_array
        return res if res else None
    
    # all Clang types should be ready for resolve_member; which means i dont believe we lazy load the EMember
    def resolve_member(self, member_path:List): # example object-var-name.something-inside
        # we want to output the EIdent, and its EMember
        if member_path:
            f = self.lookup_stack_member(member_path[0])
            if f:
                t = f.type
                m = f
                for token in member_path[1:]:
                    s = str(token)
                    t, m = t.member_lookup(t, s)
                    assert t and m, 'member lookup failed on %s.%s' % (t.get_def().name, s)
                return t, m
        return None, 0
    
    def parse_expression(self):
        self.expr_level += 1
        result = self.parse_add()
        self.expr_level -= 1
        return result
    
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
            token = self.peek_token()
            op_name = operators[op_type0 if token == op_type0 else op_type1]

            type_out = None # type conversion skipped if we dont know the type yet
            if hasattr(left, 'type') and left.type and hasattr(right, 'type') and right.type:
                if op_name in left.type.get_def().members:
                    ops = left.type.get_def().members[op_name]
                    for method in ops:
                        # check if right-type is compatible
                        assert len(method.args) == 1, 'operators must take in 1 argument'
                        if self.convertible(right.type, method.args[0].type):
                            return EMethodCall(type=method.type, target=left,
                                method=method, args=[self.convert_enode(right, method.args[0].type)])
                type_out = self.preferred_type(left, right)

            left = etype(type=type_out, left=left, right=right) # EIs and other operators may assign this type to a ERuntimeType
        return left
    
    def parse_add(self):
        return self.parse_operator(self.parse_mult,    '+', '-', EAdd, ESub)
    
    def parse_mult(self):
        return self.parse_operator(self.parse_is,      '*', '/', EMul, EDiv)
    
    def parse_is(self):
        return self.parse_operator(self.parse_eq,      'is', 'inherits', EIs, EInherits)
    
    def parse_eq(self):
        return self.parse_operator(self.parse_primary, '==', '!=', ECompareEquals, ECompareNotEquals)
    
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
        
    def type_of(self, value):
        ident = None
        if isinstance(value, ENode):
            return value.type
        elif value:
            ident = EIdent(value, module=self)
        return ident
    
    def push_context_state(self, value):
        self.context_state.append(value)

    def pop_context_state(self):
        self.context_state.pop()

    def top_context_state(self):
        return self.context_state[len(self.context_state) - 1] if self.context_state else ''
    
    def parse_sub_proc(self):
        id = self.peek_token() # may have to peek the entire tokens set for member path
        assert id == 'sub', 'expected sub keyword'
        self.consume('sub')
        ident = EIdent.parse(self) # needs to be in target of sub
        self_member = self.lookup_stack_member('self')
        self_def = self_member.type.get_def()
        method   = self_def.members[ident.ident][0]
        ctx_name = '%s_%s_ctx' % (self_def.name, method.name) 
        ctx_def  = self.defs[ctx_name]
        assert method, 'method for sub procedure not found: %s' % ident.ident
        after = self.peek_token()
        assert after == '[', 'expected context args'
        self.consume('[')
        #method = ident.get_def()
        context_args = self.parse_call_args(ident.get_def(), False, True)
        return ESubProc(type=self.defs['fn'], target=self.lookup_stack_member('self'), method=method, context_type=ctx_def.type, context_args=context_args)
        # copy context_args to inline struct data?
        # ident does not parse args.  we had to draw the line somewhere for god sakes.  it cant be some overzealous class

    # List/array is tuple
    def parse_primary(self):
        id = self.peek_token() # may have to peek the entire tokens set for member path
        print('parse_primary: %s' % (id.value))
        # suffix operators and methods
        if id == 'sub':
            return self.parse_sub_proc()

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
            ident = EIdent.parse(self)
            assert len(ident), 'expected type after typeof'
            assert self.peek_token() == ']', 'expected ] after type-identifier'
            self.consume()
            return ident

        # casting
        if id == 'cast':
            self.consume(id)
            cast_ident = EIdent.parse(self)
            assert self.peek_token() == '[', 'expected closing parenthesis on cast Type[]'
            if isinstance(cast_ident, EIdent):
                self.consume('[')
                expr   = self.parse_expression()
                self.consume(']')
                method = self.castable(expr, cast_ident)
                if method == True:
                    return EExplicitCast(type=cast_ident, value=expr)
                else:
                    return EMethodCall(type=cast_ident, target=expr, method=method, args=[])
            
                # we may still have an assignment here!
            else:
                assert False, f'cast: type not found: {id}'
        
        if id == 'ref':
            # ref is part of the type, so we must parse the full type
            # 
            self.push_token_state(self.tokens, self.index)
            ident = EIdent.parse(self)
            # lets push a context state for ref
            self.push_context_state('ref')
            if ident and not ident.members:
                # this is a ref cast
                self.transfer_token_state()
                assert ident, 'expected type-identity or member'
                member, index = self.parse_anonymous_ref(ident)
                if member:
                    self.pop_context_state()
                    return ERefCast(type=ident, value=member, index=index)
                else:
                    assert False, 'expected member'
            else:
                # and here we are getting the pointer of a value from expression
                self.pop_token_state()
                self.consume(id)
                expr = self.parse_expression() # expression should consume indexing operation but it does not
                # do we actually do this in EIdent too or is that nuts
                self.pop_context_state()
                return ERef(type=expr.type.ref_type(), value=expr)

        # we do not allow arbitrary [ code blocks ] in silver;
        # its for EStatements only, those are for methods which we can put in methods
        if id == '[':
            self.consume('[')
            expr = self.parse_expression()
            self.consume(']')
            return EParenthesis(type=etype(expr), enode=expr) # not a cast expression, just an expression
    
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
        
        self.push_token_state(self.tokens, self.index)
        ident = EIdent.parse(self)

        # dynamic construct
        dynamic_construct = ident and ident.meta_member and self.peek_token() == '['
        if ident and ident.members and not dynamic_construct:
            # ident is a member identity
            self.transfer_token_state()

            if ident.meta_member: # we have EMetaMember in stack bound to this ident.meta_member
                # its not enough to return here:
                # we need to perform .member lookups as methods
                # it would be nice to incorporate it below; based on meta_member it changes the operation to find the location of a method and call, rather than invoke it
                return ident

            edef = ident.get_def()
            imember = ident.members[0] # dont believe we need to move through these unless its a method

            # check if member is a method, and we are calling it
            # todo: support methods without [ parens ] ... this would require a 'ref' before a method for obtaining reference to it
            is_method = isinstance(imember, EMethod)
            is_open   = self.peek_token() == '['
            is_ref    = self.top_context_state() == 'ref' # i think just the top is what we want here; it may need to be broken up by another mode

            if is_method and is_ref:
                assert not is_open, 'expected no method parenthesis'
                # EMethod type on ERef tells us something.  We want a method pointer
                # we call ref method-name  ... we expect no parenthesis
                return ERef(type=EIdent(imember, module=self), value=imember)
            if is_method or is_open:
                if is_open:
                    self.consume('[')
                else:
                    assert self.expr_level == 0, 'method calls beyond statement require parenthesis'
                method = None
                if not is_method:
                    # now we must check for index methods
                    enode_args = self.parse_call_args(None)
                    arg_count = len(enode_args)
                    member_def = imember.type.get_def()
                    if not 'index' in member_def.members:
                        assert member_def.model.name != 'allocated', 'no indexing method found on this type'
                        assert len(enode_args) == 1, 'too many arguments for primitive indexing'
                        assert imember.type.ref() > 0, 'attempting to index a primitive value [not a ref]'
                        return EIndex(type=imember.type.ref_type(-1), target=imember, value=enode_args[0])
                    convertible  = None
                    for index_method in member_def.members['index']:
                        arg_keys = list(index_method.args.keys())
                        m_count = len(arg_keys)
                        if arg_count != m_count: continue
                        can_convert = 0
                        same_types  = 0
                        for name, arg in index_method.args.items():
                            arg_type = arg.type
                            arg_conv = self.convertible(enode_args[can_convert].type, arg.type)
                            is_same  = enode_args[can_convert].type.get_base() == arg.type.get_base()
                            if not arg_conv: break
                            can_convert += 1
                            if is_same: same_types += 1
                        if not convertible and can_convert == arg_count:
                            convertible = index_method
                        elif same_types == arg_count:
                            method = index_method
                            break
                    if not method:
                        method = convertible
                    assert method, 'no-suitable indexing method found on member type %s' % member_def.name
                else:
                    method = imember
                    enode_args = self.parse_call_args(imember)
                
                if is_open:
                    self.consume(']')
                conv_args      = self.convert_args(method, enode_args)
                return EMethodCall(type=ident, target=module.lookup_stack_member(method.access), method=method, args=conv_args)
            else:
                return imember # todo: we need more context than this
        else:
            type_def = ident.get_def()
            if isinstance(type_def, EMetaMember):
                # can be construction, or a method call
                self.transfer_token_state()
                token_after = self.peek_token()
                if token_after == '[':
                    self.consume('[')
                    enode_args = self.parse_call_args(None)
                    self.consume(']')
                    if ident.members:
                        last_member = ident.members[len(ident.members) - 1]
                        target = ident.members[len(ident.members) - 2] if len(ident.members) > 1 else self.lookup_stack_member('self')
                        return EMethodCall(type=self.defs['object'].type, target=target, method=last_member, args=enode_args)
                    else:
                        return EConstruct(type=self.defs['object'].type, meta_member=type_def, method=None, args=enode_args) # at design time, we may handle object differently
                else:
                    return ident # it could be beneficial to put the EMetaMember where EIdent goes, the type field; however we must make it actually inherit
            elif type_def:
                # ident is a type
                self.transfer_token_state() # this is a pop and index update; it means continue from here (we only do this if we are consuming this resource)
                token_after = self.peek_token()
                if token_after == '[':
                    self.consume()
                    if self.is_method(ident):
                        enode_args = self.parse_call_args(type_def)
                        assert type_def.type, 'type required on EMethod'
                        return EMethodCall(type=type_def.type, target=type_def.access, method=type_def, args=enode_args)
                    else:
                        # construction
                        edef = ident.get_def()
                        #prim = is_primitive(ident)
                        enode_args = self.parse_call_args(edef.members['ctr']) # we need to give a signature list of the constructors
                        conv = enode_args
                        method = None
                        # if args are given, we use a type-matched constructor (first arg is primary)
                        if len(enode_args):
                            method = self.constructable(enode_args[0], ident)
                            assert method, 'cannot construct with type %s' % enode_args[0].type.get_def().name
                            conv = self.convert_args(method, enode_args)
                        # return construction node; if no method is defined its a simple new and optionally initialized
                        return EConstruct(type=ident, method=method, args=conv)
                else:
                    #self.consume()
                    assert ident.ident and not ident.members, 'expected a type'
                    return ident
            
            elif is_alpha(id):
                assert False, 'unknown identifier: %s' % id
            
            # expect chars like ]
            self.pop_token_state() # we may attempt to parse differently now
        
        return None
    
    def reset_member_depth(self):
        self.member_stack = [OrderedDict()]
    
    def push_member_depth(self):
        self.member_stack.append(OrderedDict())

    def pop_member_depth(self):
        return self.member_stack.pop()

    def push_return_type(self, type:EIdent):
        assert('.return-type' not in self.member_stack[-1])
        self.member_stack[-1]['.return-type'] = [EMember(type=type, name='.return-type')]

    def push_member(self, member:EMember, access:EMember = None):
        top = self.member_stack[-1]
        assert(member.name not in top)
        if not member.name in top:
            top[member.name] = []
        top[member.name].append(member)

    def casts(self, enode_or_etype):
        try:
            return etype(enode_or_etype).get_def().members['cast']
        except:
            return None
            
    def constructs(self, enode_or_etype):
        try:
            return etype(enode_or_etype).get_def().members['ctr']
        except:
            return None
        
    # we have 3 types here:
    # EIdent
    #
    # clang-tuple-identity [with TypeKind + EIdent <-- TypeKind not needed if we are using EIdent]

    def castable(self, fr:EIdent, to:EIdent):
        fr = etype(fr)
        #to = etype(to)
        assert isinstance(fr, EIdent), 'castable fr != EIdent'
        assert isinstance(to, EIdent), 'castable to != EIdent'
        ref = fr.ref_total()
        if (ref > 0 or self.is_primitive(fr)) and to.get_base().name == 'bool':
            return True
        # type handled in C99 emitter :: bool method format would be checked in EMethod; 
        # if method is a type, then its a cast.  if method is a string name, 
        # its a method you can key in the methods map; its probably a decent 
        # idea to map the casts
        fr = etype(fr)
        cast_methods = self.casts(fr)
        if fr == to or (self.is_primitive(fr) and self.is_primitive(to)):
            return True
        
        # code is a bit ugly in the EIdent area, and we are repeating argument emitting/conversion in various places
        # 
        if self.is_primitive(fr):
            if to.get_def() == self.defs['object']:
                return True

        # we should have constructs across object for creating with various primitives, however that pollutes the polymorphic landscape quite a bit
        a_ctr = self.constructs(to)
        # go through constructors
        for member in a_ctr:
            arg_keys = list(member.args.keys())
            assert len(arg_keys) > 0, 'invalid constructor'
            first = member.args[arg_keys[0]]
            if self.convertible(fr, first.type):
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
        b = self.defs['b']
        ib = to.get_def()
        ctrs = self.constructs(to) or {}
        if fr == to:
            return True
        for method in ctrs:
            assert len(method.args), 'constructor must have args'
            _, f = first_key_value(method.args)
            if f.type == fr:
                return method
        return None

    def convertible(self, efrom, to):
        fr = etype(efrom)
        assert isinstance(efrom, EIdent), 'convertible fr != EIdent'
        assert isinstance(to,    EIdent), 'convertible to != EIdent'
        if fr == etype: return True
        return self.castable(fr, to) or self.constructable(fr, to)

    # name these functions better
    def convert_enode(self, enode, type):
        assert enode.type, 'code has no type. thats trash code'
        if enode.type != type:
            cast_m      = self.castable(enode, type)
            # if converting to object, we sometimes wrap a primitive, otherwise its no operation
            # its an illegal operation to cast 'object' on an object, which makes logical space for wrapping:
            if self.is_primitive(enode.type) and type.get_base() == self.defs['object']:
                return EPrimitive(type=type, value=enode)

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
        
    # we are converting through emit logic above, however its proper to do it in enode
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
            
    def parse_while(self, t0):
        self.next_token()  # Consume 'while'
        assert self.peek_token() == '[', "expected condition expression '['"
        self.next_token()  # Consume '['
        condition = self.parse_expression()
        assert self.next_token() == ']', "expected condition expression ']'"
        self.push_member_depth()
        statements = self.parse_statements()
        self.pop_member_depth()
        return EWhile(type=None, condition=condition, body=statements)
            
    def parse_do_while(self, t0):
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
    
    def parse_if_else(self, t0):
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

    def parse_for(self, t0):
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

    def parse_break(self, t0):
        self.next_token()  # Consume 'break'
        levels = None
        if self.peek_token() == '[':
            self.next_token()  # Consume '['
            levels = self.parse_expression()
            assert self.peek_token() == ']', "expected ']' after break[expression...]"
            self.next_token()  # Consume ']'
        return EBreak(type=None, levels=levels)
    
    def parse_return(self, t0):
        self.next_token()  # Consume 'return'
        result = self.parse_expression()
        rmember = self.lookup_stack_member('.return-type')
        assert rmember, 'no .return-type record set in stack'
        return_obj = EMethodReturn(type=rmember.type, value=result)
        return return_obj 
    
    def parse_anonymous_ref(self, ident):
        member = None
        indexing_node = None
        if self.peek_token() == '[':
            # ref Type [ member ] [ offset-expr ]
            assert ident.ref(), 'expecting anonymous reference'
            self.consume('[')
            member = self.parse_expression()
            if isinstance(member, EProp):
                # with anonymous ref, we lose constant state by design
                member = EProp(
                    name=member.name, type=member.type, module=self,
                    value=member.value, parent=member.parent,
                    visibility=member.visibility, is_prop=True, args=member.args)
            assert member, 'type not found: %s' % ident.ident
            self.consume(']')
            
            # parse optional indexing dimension
            if self.peek_token() == '[':
                self.consume('[')
                indexing_node = self.parse_expression()
                self.consume(']')
        return member, indexing_node
    
    def parse_statement(self):
        t0    = self.peek_token()
        
        if t0 == 'return': return self.parse_return(t0)
        if t0 == 'break':  return self.parse_break(t0)
        if t0 == 'for':    return self.parse_for(t0)        
        if t0 == 'while':  return self.parse_while(t0)
        if t0 == 'if':     return self.parse_if_else(t0)
        if t0 == 'do':     return self.parse_do_while(t0)
    
        if t0 == 'udata1':
            self

        if t0 == 'array':
            self
            
        ident    = EIdent.parse(self) # should parse nothing if there is nothing to parse..  new rule anyway..
        type_def = ident.get_def()    # also try not to over-parse with EIdent.  the consumables might be over doing it
        if type_def or ident.members: # [ array? ]
            # simple method call
            if not ident.members and isinstance(type_def, EMethod):
                return self.parse_expression()
            after = self.peek_token()
            
            # we need a parse member since this is parse statement; we will be reading members elsewhere
            not_member_or_ref = not ident.members and not ident.ref()
            if after == '[' and (not_member_or_ref or ident.members):
                if ident.meta_member and ident.members: # I.method
                    # we parse args the same but we do not convert, tahts for runtime's method_call to perform
                    # if we want named arguments we actually have to perform a bit of dancing around
                    # its obviously not as advised as simply giving typed values in sequence, but it should be there to facilitate a full feature set
                    method_str = ident.members[len(ident.members) - 1]
                    self.consume('[')
                    args = self.parse_call_args(method_str) # List[ENode]
                    self.consume(']')
                    return EMethodCall(type=None, target=ident, method=ident, args=args)
                elif ident.members:
                    # call method
                    method_edef = ident.members[len(ident.members) - 1]
                    self.consume('[')
                    args = self.parse_call_args(method_edef) # List[ENode]
                    self.consume(']') # i64 to object
                    conv = self.convert_args(method_edef, args) # args must be the same count and convertible, type-wise
                    return EMethodCall(type=method_edef.type, target=ident.get_target(), method=method_edef, args=conv)
                else:
                    # we cannot construct anonymous instances since its typically a mistake when we do, and they are easy to discover
                    assert False, 'compilation error: unassigned instance'
            else:
                declare = False
                
                if ident.members and not ident.meta_member:
                    assign = is_assign(after)
                    require_assign = False
                    index = 0
                    while True:
                        if len(ident.members) <= index:
                            assert False, 'writable member not found'
                            break
                        member = ident.members[index]
                        if member.access != 'const' or not assign:
                            break
                        index += 1
                    indexing_node = None # should be part of EIdent
                else:
                    # anonymous assignment by reference:
                    member, indexing_node = self.parse_anonymous_ref(ident)
                    after = self.peek_token()
                    # declaration:
                    if not member or ident.meta_member:
                        require_assign = False
                        if is_alpha(after):
                            # variable declaration
                            self.consume(after)
                            declare = True
                            generic = self.defs['object']
                            member  = EMember(name=str(after), access=None, type=generic.type if ident.meta_member else ident,
                                value=None, visibility='public')
                            member._is_mutable = True
                            after   = self.peek_token()
                            #after  = self.peek_token()
                        else:
                            assert False, 'not handled'
                    else:
                        require_assign = True

                assign = is_assign(after)
                if declare:
                    self.push_member(member)
                if assign:
                    # we need to actually change the member for assignments when we encounter a const; 
                    # 
                    if not member.access: # we need both object name for access and its read/write access
                        assert not hasattr(member, '_is_mutable') or member._is_mutable, 'internal member state error (no EMember access set on registered member)'
                        member.access = 'const' if assign == '=' else None
                    self.consume(assign)
                    value = self.parse_expression()
                    type = ident if not ident.members else type_def.type

                    return assign_map[assign](
                        type=type, target=member,
                        index=indexing_node, declare=declare,
                        value=value)
                else:
                    assert not require_assign, 'assign required after anonymous reference at statement level'
                    return EDeclaration(type=member.type, target=member)
        else:
            return self.parse_expression()

    def parse_statements(self, prestatements = None):
        if prestatements != None:
            block = prestatements.value
        else:
            block = []  # List to hold enode instances
        multiple = self.peek_token() == '['

        tokens, index = self.debug_tokens()
        if multiple:
            self.next_token()  # Consume '['

        depth = 1
        self.push_member_depth()
        while self.peek_token():
            t = self.peek_token()
            if multiple and t == '[':    # here, this is eating my cast <----------- 
                depth += 1
                self.push_member_depth()
                self.consume()
                continue
            global debug
            debug += 1
            n = self.parse_statement()  # Parse the next statement
            assert n is not None, 'expected statement or expression'
            block.append(n)
            if not multiple: break
            if multiple and self.peek_token() == ']':
                if depth > 1:
                    self.pop_member_depth()
                self.next_token()  # Consume ']'
                depth -= 1
                if depth == 0:
                    break
        self.pop_member_depth()
        # Return a combined operation of type EType_Statements
        return prestatements if prestatements else EStatements(type=None, value=block)

    # handle named arguments here
    def parse_call_args(self, signature:EMethod, C99:bool = False, context:bool = False): # needs to be given target args
        if isinstance(signature, list):
            signature = None # todo: handle this case, where we match based on the first argument [from constructors listing]
        elif isinstance(signature, str):
            signature = None # we only know the name of the method, but thats enough for run-time
        e = ')' if C99 else ']'

        enode_args = []
        arg_index  = 0
        if context:
            assert signature, 'signature must be provided for context args'
            arg_count = len(signature.context_args)
        else:
            arg_count = len(signature.args) if signature else -1
        
        while (1):
            after = self.peek_token()
            if after == ']' or after == '::':
                assert arg_count == -1, 'not enough arguments given to %s' % signature.name
                break
            op = self.parse_expression()
            after = self.peek_token()
            enode_args.append(op)
            arg_index += 1
            if after == ',':
                if arg_count > 0 and arg_index >= arg_count:
                    assert False, 'too many args given to %s' % signature.name
                self.consume()
            else:
                break
        self.assertion(self.peek_token() == e, 'expected ] after args')
        return enode_args
    
    # needs to work with 
    def parse_defined_args(self, is_no_args=False, is_C99=False):
        b = '(' if is_C99 else '['
        e = ')' if is_C99 else ']'
        args:OrderedDict[str, EProp] = OrderedDict()
        context:OrderedDict[str, EProp] = OrderedDict()
        if self.peek_token() == e:
            return args, context
        arg_type = None if is_no_args else EIdent.peek(self)
        if is_C99:
            is_C99
        n_args = 0
        inside_context = False
        while arg_type:
            arg_type = EIdent.parse(self)
            assert arg_type, 'failed to parse type in arg'
            arg_name_token = str(n_args) if is_C99 else None
            if not is_C99 and is_alpha(self.peek_token()):
                arg_name_token = self.next_token()
            if not is_C99:
                assert is_alpha(arg_name_token), 'arg-name (%s) read is not an identifier' % (arg_name_token)
            arg_name = str(arg_name_token)
            arg_def = arg_type.get_def()
            assert arg_def, 'arg could not resolve type: %s' % (arg_type[0].value)
            ar = context if inside_context else args
            ar[arg_name] = EProp(
                name=str(arg_name_token), access='const' if inside_context else 'const',
                type=arg_type, value=None, visibility='public')
            n = self.peek_token()
            if n == ',':
                self.next_token()  # Consume ','
            elif n == '::':
                self.next_token()  # Consume ','
                inside_context = True
            else:
                assert n.value == e
            n = self.peek_token()
            if not n in consumables and not is_alpha(n):
                break
            n_args += 1
        return args, context

    def finish_method(self, cl, token, method_type, is_static, visibility):
        is_ctr     = method_type == 'ctr'
        is_cast    = method_type == 'cast'
        is_index   = method_type == 'index'
        is_no_args = method_type in ('init', 'dealloc', 'cast')
        if   is_cast:  self.consume('cast')
        #prev_token()
        t = self.peek_token()
        has_type = is_alpha(t)
        return_type = EIdent.parse(self) if has_type else EIdent(value=cl.name, module=self) # read the whole type!
        name_token  = self.next_token() if (method_type in ['method', 'index']) else None
        if not has_type:
            self.consume(method_type)
        t = self.next_token() if not is_no_args else '['
        assert t == '[', f"Expected '[', got {token.value} at line {token.line_num}"
        args, context = self.parse_defined_args(is_no_args) # context args should define a struct we use for storage
        if not is_no_args:
            n = self.next_token()
            assert str(n) in [']', '::'], 'expected end of args'

        t       = self.peek_token()
        is_auto = t == 'auto'
        body    = None
        if not is_auto:
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
        else:
            self.consume('auto')
                
        #else:
        #   body = parse_expression_tokens -- do this in 1.0, however its difficult to lazy load this without error
        
        if method_type == 'method':
            id = str(name_token)
        else:
            id = method_type
        #elif is_cast:
        #    id = 'cast_%s' % str(return_type[0])
        #else:
        #    id = method_type
        arg_keys = list(args.keys())
        if is_index and not arg_keys:
            assert not is_index or arg_keys, 'index methods require an argument'
        
        if is_cast:
            name = 'cast_%s' % str(return_type.ident)
        elif is_index:
            arg_types = ''
            for _, a in args.items():
                arg_type   = a.type.get_name()
                arg_types += '_%s' % arg_type
            name = 'index%s' % arg_types
        elif is_ctr:
            assert len(arg_keys), 'constructors must have something to construct-with'
            name = 'with_%s' % str(args[arg_keys[0]].type.get_name())
        else:
            name = id

        if context:
            # create def for this named cl.name '_' + name + '_' + context
            keys      = list(context.keys())
            first_arg = context[keys[-1]]
            ctx_name  = '%s_%s_ctx' % (cl.name, str(name_token))
            ctx       = self.defs[ctx_name] = EClass(name=ctx_name, module=self, model=models['allocated'], visibility='intern')
            ctx_type  = EIdent(ctx, module=self)
            ctx.type  = ctx_type
            ctx_statements = EStatements(type=None, value=[])
            target = EMember('self', type=ctx_type, module=self, visibility='public')
            for arg_name, arg in context.items():
                ctx_prop = EProp(name=arg.name, type=arg.type, access='self', module=self, visibility='public')
                ctx.members[arg.name] = [ctx_prop]
                # all of these assignments come from the constructor arguments, so they must all 'grab'
                ctx_statements.value.append(EAssign(type=ctx_prop.type, target=ctx_prop, value=arg, index=None))
            name = id
            # constructor with all members; 
            ctx.members['ctr'] = [
                EMethod('with_%s' % first_arg.type.get_name(),
                        type=ctx_type, visibility='public', type_expressed=True, statements=ctx_statements, args=context)
            ]
        
        # we need a way of reading the public/intern state
        method = EMethod(
            name=name, type=return_type, method_type=method_type, value=None, access='self', parent=cl, type_expressed=has_type,
            static=is_static, args=args, context_args=context, body=body, visibility=visibility, auto=is_auto)
    
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

        # convert the basic meta_model of strings to a resolved one we call meta_types
        if cl.meta_model:
            cl.meta_types = OrderedDict()
            for symbol, conforms in cl.meta_model.items(): # we are not supporting conform
                edef_conforms = None
                if conforms:
                    edef_conforms = self.defs[conforms] if conforms in self.defs else None
                    assert edef_conforms, 'no definition found for meta argument %s' % conforms
                cl.meta_types[symbol] = EIdent(symbol, module=self, conforms=edef_conforms) # anyone querying can check their given type against this conform

        while (token := self.peek_token()) != ']':
            is_static = token == 'static'
            if is_static:
                self.consume('static')
                token = self.peek_token()
            visibility = 'public'
            if str(token) in ('public', 'intern'):
                visibility = str(token)
                token = self.next_token()
            if not is_static:
                is_static = token == 'static'
                if is_static:
                    self.consume('static')
                    token = self.peek_token()
            
            is_method0 = False
            self.push_token_state(self.tokens, self.index)
            ident = EIdent.parse(self)

            if token == 'cast':
                assert not is_static, 'cast must be defined as non-static'
                is_method0 = True
            elif token == cl.name:
                assert not is_static, 'constructor must be defined as non-static'
                is_method0 = True
            else:
                is_method0 = ident and self.peek_token(1) == '['
                token = self.peek_token()
                global no_arg_methods
                if token.value in no_arg_methods: is_method0 = True # cast and init have no-args
            
            self.pop_token_state()

            if not self.is_defined([Token(value='bool')]):
                self.is_defined([Token(value='bool')])

            if not is_method0:
                # property
                ident = EIdent.parse(self)
                name_token  = self.next_token()
                next_token_value = self.peek_token().value
                if next_token_value == ':':
                    self.next_token()  # Consume the ':'
                    value_token = self.next_token()
                    prop_node = EProp(
                        type=ident, name=name_token.value, access='self',
                        value=value_token.value, visibility=visibility, parent=cl)
                else:
                    prop_node = EProp(
                        type=ident, name=name_token.value, access='self',
                        value=None, visibility=visibility, parent=cl)
                id = str(name_token)
                #assert not id in cl.members, 'member exists' # we require EMethod != EProp checks too
                if not id in cl.members:
                    cl.members[id] = []
                cl.members[id].append(prop_node)
                # being compatible with 2 or more member functions would be considered 'ambiguous' to silver
                # silver does require direct cast, or construction
                # not sure if we extend construction to public members
            elif token == cl.name: self.finish_method(cl, token, 'ctr', is_static, visibility)
            elif str(token) in no_arg_methods: self.finish_method(cl, token, str(token))
            elif token == 'cast':  self.finish_method(cl, token, 'cast',   is_static, visibility)
            elif token == 'index': self.finish_method(cl, token, 'index',  is_static, visibility)
            else: self.finish_method(cl, token, 'method', is_static, visibility)
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

    def parse_import(self, visibility) -> 'EImport':
        result = EImport(name='', source=[], includes=[], visibility=visibility)
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

    def parse_class(self, visibility, meta):
        token = self.next_token()
        assert token == 'class', f"Expected 'class', got {token.value} at line {token.line_num}"
        name_token = self.next_token()
        if 'app' in self.defs:
            'found an app'
        is_ty = self.is_defined([name_token])
        assert not is_ty, 'duplicate definition in module'

        assert name_token is not None
        class_node = EClass(module=self, name=name_token.value, visibility=visibility, meta_model=meta)
        # we resolve meta this when we finish

        # set 'model' of class -- this is the storage model, with default being an allocation (heap)
        # model handles storage and primitive logic
        # for now only allocated classes have 
        class_node.model = models['allocated'] # we dont need the user to be able to access this, as its internal

        if self.peek_token() == ':':
            self.consume()
            class_node.inherits = self.find_def(self.next_token())
        
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
        prestatements = None

        if method.auto:
            enodes = []
            prestatements = EStatements(type=method.type, value=enodes)
            for key, member_arg in method.args.items():
                assert key in class_def.members, '%s does not exist in class %s' % (str(key), class_def.name)
                a_members = class_def.members[key]
                first = a_members[0]
                assert isinstance(first, EProp), 'member must be a member variable'
                can_conv = self.convertible(member_arg.type, first.type)
                assert can_conv, 'cannot convert member %s' % (str(key))
                enodes.append(EAssign(type=first.type, target=first, value=member_arg, index=None, declare=False))
        elif method.statements != None:
            prestatements = method.statements
        else:
            assert len(method.body) > 0
        
        # since translate is inside the read_module() method we must have separate token context here
        self.push_token_state(method.body if method.body else [])
        #self.reset_member_depth()
        assert len(self.member_stack) == 1
        
        self.push_member_depth()
        self.push_return_type(method.type)

        # when we parse our class, however its runtime we dont need to call the method; its on the instance
        for name, a_members in class_def.members.items():
            for member in a_members:
                if member.name != 'index':
                    self.push_member(member) # if context name not given, we will perform memory management (for args in our case)
        
        self.push_member_depth()
        self.current_def = class_def # we will never translate more than 1 at a time
        self_ident = EIdent(class_def, module=self)
        if not method.static:
            self.push_member(EMember('self', type=self_ident, module=self, visibility='public'))
        if method.context_args:
            self.push_member(EMember('_ctx', type=None, module=self, visibility='public'))
            for name, arg_member in method.context_args.items():
                self.push_member(arg_member)
        for name, member in method.args.items():
            self.push_member(member)
        
        # we need to push the args in, as well as all class members (prior, so args are accessed first)
        enode = self.parse_statements(prestatements)
        assert isinstance(enode, EStatements), 'expected statements'
        
        # self return is automatic -- however it might be nice to only do this when the type is not expressed by the user
        if (not method.type_expressed and method.type 
            and method.type.get_def() == class_def
            and isinstance(enode.value[len(enode.value) - 1], EMethodReturn)):
            enode.value.append(EMethodReturn(type=method.type, value=self.lookup_stack_member('self')))

        self.pop_member_depth()
        self.pop_member_depth()
        self.pop_token_state()
        global vebose
        if verbose:
            print('printing enodes for method %s' % (method.name))
            print_enodes(enode)
        assert len(self.member_stack) == 1
        return enode
    
    def parse_meta_model(self):
        assert self.next_token() == 'meta', 'meta keyword expected'
        assert self.next_token() == '[',    '[ expected after meta'
        result:OrderedDict[str, str] = OrderedDict()
        while True:
            symbol   = self.next_token()
            if symbol == ']': break
            after    = self.peek_token()
            conforms = None
            if after == ':':
                self.consume(':') # these are classes or protos
                conforms = self.next_token()
                # assert conforms in self.defs -- we can only do this after reading the module
            result[symbol] = conforms
            after = self.peek_token()
            if after == ',':
                self.consume(',')
            else:
                assert after == ']', 'expected ] after meta symbol:conforms'
                self.consume(']')
                break
            if symbol == ']': break
        assert len(result), 'meta args required for meta keyword'
        return result

    def parse(self, tokens:List[Token]):
        self.push_token_state(tokens)
        # top level module parsing
        visibility = ''
        meta_model:OrderedDict = None
        while self.index < len(self.tokens):
            token = self.next_token()
            
            if token == 'meta':
                assert not visibility, 'visibility applies to the class under meta keyword: [public, intern] class'
                self.index -= 1
                meta_model = self.parse_meta_model()
                continue

            elif token == 'class':
                if not visibility: visibility = 'public' # classes are public by default
                self.index -= 1  # Step back to let parse_class handle 'class'
                class_node = self.parse_class(visibility, meta_model) # replace with a map at 3, since we are now gathering top level details
                assert not class_node.name in self.defs, '%s duplicate definition' % (class_node.name)
                self.defs[class_node.name] = class_node
                visibility = ''
                meta_model = None
                continue

            elif token == 'import':
                assert not meta_model, 'meta not applicable'
                if not visibility: visibility = 'intern' # imports are intern by default
                self.index -= 1  # Step back to let parse_class handle 'class'
                import_node = self.parse_import(visibility)
                assert not import_node.name in self.defs, '%s duplicate definition' % (import_node.name)
                self.defs[import_node.name] = import_node
                import_node.process(self)
                visibility = ''
                meta_model = None
                continue

            elif token == 'public': visibility = 'public'
            elif token == 'intern': visibility = 'intern'
            else: assert False, 'unexpected token in module: %s' % str(token)

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
        # initialize types, top level parsing, and then proceed to build dependencies used by the module
        self.initialize()              # define primitives first
        self.parse(self.tokens)        # parse tokens
        self.build_dependencies()      # compile things imported, before we translate
        # now we graph the methods
        for name, enode in self.defs.items():
            if isinstance(enode, EClass) and not enode.imported:
                # meta is set of runtime members of the symbol given
                # the exception to this is when its used in place of a type
                # in which case we use a base A-type
                self.push_member_depth()
                index = 0
                if enode.meta_types:
                    for symbol, edef_conforms in enode.meta_types.items():
                        member = EMetaMember(name=str(symbol), parent=enode, index=index, module=self, conforms=edef_conforms) # this resolves at runtime based on the index of our type in the class's type.meta list
                        self.push_member(member)
                        index += 1
                # graph methods
                for name, a_members in enode.members.items():
                    for member in a_members:
                        if isinstance(member, EMethod):
                            member.code = self.translate(class_def=enode, method=member)
                            self.member_stack
                            print('method: %s' % name)
                            print_enodes(member.code)
                self.pop_member_depth()
            elif isinstance(enode, EImport):
                pass
        # check for app class, indicating entry point
        app = self.defs['app'] if 'app' in self.defs else None
        if verbose and app: app.print()
        if not self.emit(app_def=app):
            print('error during C99 emit')
            exit(1)
        # build the emitted C99 source
        # its hubris to think we can catch everything in member parsing and reverse descent
        # its possible, though and certainly testable
        self.build()

# all of the basic types come directly from A-type implementation
# string, array, map, hash-map etc
# we need to import these, still (with meta types)

# silver44 (0.4.4) is python, but 1.0.0 will be C (lol maybe)
def parse_bool(v):
    if isinstance(v, bool): return v
    s = v.lower()
    if s == 'true'  or s == 'yes' or s == '1': return True
    if s == 'false' or s == 'no'  or s == '0': return False
    raise argparse.ArgumentTypeError('invalid boolean format: %s' % str(v))

# setup arguments
build_root   = os.getcwd()
parser       = argparse.ArgumentParser(description="Process some flags.")
parser.add_argument('--verbose', default=verbose,    type=parse_bool, help='Increase output verbosity', const=True, nargs='?')
parser.add_argument('--debug',   default=is_debug,   type=parse_bool, help='compile with debug info',   const=True, nargs='?')
parser.add_argument('--cwd',     default=build_root, type=str,        help='path for relative sources (default is current-working-directory)')
args, source = parser.parse_known_args()

# print usage if not using
if not source:
    print( 'silver 0.44')
    print(f'{"-" * 32}')
    print( 'source be given as file/path/to/silver.si')
    parser.print_usage()

# update members from args
verbose       = args.verbose
build_root    = args.cwd

# build the modules provided by user
for i in range(len(source)):
    src           = source[i]
    module        = EModule(path=Path(src)) # path is relative from our build root (cwd)