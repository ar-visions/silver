from dataclasses import dataclass, field, fields, is_dataclass
from typing import Type, List, OrderedDict, Tuple, Any
from pathlib import Path
import numpy as np
import os

class Token:
    def __init__(self, value, line_num):
        self.value = value
        self.line_num = line_num

    def split(ch):
        return self.value.split(ch)

    def __str__(self):
        return self.value
    
    def __repr__(self):
        return f"Token({self.value}, line {self.line_num})"
    
    def __getitem__(self, key):
        return self.value[key]
    
    def __eq__(self, other):
        if isinstance(other, Token):
            return self.value == other.value
        elif isinstance(other, str):
            return self.value == other
        return False

def tokens(input_string):
    special_chars = "$,<>()![]/+*:=#"
    tokens = []
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

_next_id = 0  # Class variable to keep track of the next available ID

@hashify
@dataclass
class ENode:
    def __init__(self):
        self.id = _next_id
        _next_id += 1
    
    def __repr__(self):
        attrs = ', '.join(f"{key}={value!r}" for key, value in vars(self).items())
        return f"{type(self).__name__}({attrs})"
    
    def enodes(self):
        return [], [] # key or value? 
    
    def to_serialized_string(self):
        attrs = ', '.join(f"{key}={self.serialize_value(value)}" for key, value in vars(self).items())
        return f"{type(self).__name__}({attrs})"

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

    # now we need to mark the EMethodCall with a reference, so the consumer of method call may use it
    
    # replace nodes with helper lambda
    def enode_replace(self, fn, ctx):
        r = fn(self, ctx)
        if r: return r
        for f in fields(self):
            inst = getattr(self, f.name)
            if isinstance(inst, ENode):
                r = fn(inst, ctx)
                if r: setattr(self, field.name, r)
            elif isinstance(inst, List[ENode]):
                index = 0
                for e in inst:
                    r = fn(e, ctx)
                    if r: inst[index] = r
                    index += 1
            elif isinstance(inst, OrderedDict[str, ENode]):
                for key in inst:
                    r = fn(inst[key], ctx)
                    if r: inst[key] = r
        return self
    # method_calls = enode.each(EMethodCall)

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

@hashify
@dataclass
class EModule(ENode):
    name:       str
    path:       Path = None
    imports:    OrderedDict[str, 'ENode'] = field(default_factory=OrderedDict)
    defs:       OrderedDict[str, 'ENode'] = field(default_factory=OrderedDict)
    type_cache: OrderedDict[str, 'EType'] = field(default_factory=OrderedDict)
    id:int = None
    def parse(self, tokens:List[Token]):
        parse_tokens(module=self, _tokens=tokens)
    def find_class(self, class_name):
        class_name = str(class_name)
        for iname, enode in self.imports:
            if isinstance(enode, 'EModule'):
                f = enode.find_class(class_name)
                if f: return f
        if class_name in self.defs: return self.defs[class_name]
        return None
    def emit(self):
        module_emit(self, '%s.c' % self.name)

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

@hashify
@dataclass
class EClass(ENode):
    module: EModule
    name: str
    model: EModel = None
    inherits: 'EClass' = None
    block_tokens: List[Token] = field(default_factory=list)
    meta_types:List[ENode] = field(default_factory=list)
    members:OrderedDict[str, List['EMember']] = field(default_factory=OrderedDict)
    id:int = None

@hashify
@dataclass
class EType(ENode):
    definition: EClass # eclass is resolved here, and we form
    meta_types: List['EType'] = field(default_factory=list)
    id:int = None

@hashify
@dataclass
class EMember(ENode):
    name: str
    type: EType
    value: ENode
    access: str
    visibility: str
    def enodes(self):
        return [self.value]

@hashify
@dataclass
class EMethod(EMember):
    body:List[Token]
    args:OrderedDict[str,EMember]
    code:ENode = None
    
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
class EProp(EMember):
    pass

@hashify
@dataclass
class EUndefined(ENode):
    pass

@hashify
@dataclass
class EParenthesis(ENode):
    type:EType
    enode:ENode

@hashify
@dataclass
class EAssign(ENode):
    type:EType
    target:ENode
    value:ENode

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
    body:List[Token]
    else_body:ENode

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

@hashify
@dataclass
class EDoWhile(ENode):
    type: EType     # None
    condition:ENode
    body:ENode

@hashify
@dataclass
class EBreak(ENode):
    type: EType     # None
    levels:int

@hashify
@dataclass
class ELiteralReal(ENode):
    type: EType     # f64
    value:float

@hashify
@dataclass
class ELiteralInt(ENode):
    type: EType     # i64
    value:int

@hashify
@dataclass
class ELiteralStr(ENode):
    type: EType     # string
    value:str

def value_for_type(type, token):
    if type == ELiteralStr:  return token.value # token has the string in it
    if type == ELiteralReal: return float(token.value)
    if type == ELiteralInt:  return int(token.value)

@hashify
@dataclass
class ELiteralStrInterp(ENode):
    type: EType     # string
    value:str

#@hashify
#@dataclass
#class EReference(ENode):
#    member:EMember

@hashify
@dataclass
class EAdd(ENode):
    type:EType
    left:ENode
    right:ENode

@hashify
@dataclass
class ESub(ENode):
    type:EType
    left:ENode
    right:ENode

@hashify
@dataclass
class EMul(ENode):
    type:EType
    left:ENode
    right:ENode

@hashify
@dataclass
class EDiv(ENode):
    type:EType
    left:ENode
    right:ENode

@hashify
@dataclass
class EOr(ENode):
    type:EType
    left:ENode
    right:ENode

@hashify
@dataclass
class EAnd(ENode):
    type:EType
    left:ENode
    right:ENode

@hashify
@dataclass
class EXor(ENode):
    type:EType
    left:ENode
    right:ENode

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

@hashify
@dataclass
class EMethodReturn(ENode): # v1.0 we will want this to be value:List[ENode]
    type:EType
    value:ENode
    def enodes(self):
        return [self.value]
    def __hash__(self):
        return hash(tuple(_make_hashable(getattr(self, field.name)) for field in fields(self)))

@hashify
@dataclass
class EStatements(ENode):
    type:EType # last statement type
    value:List[ENode]
    def enodes(self):
        return self.value

keywords = [ "class",  "proto", "struct", "import", 
             "return", "asm",   "if",     "switch", "while", "for", "do" ]
assign   = [ ':',  '+=', '-=',  '*=',  '/=', '|=',
             '&=', '^=', '>>=', '<<=', '%=' ]

def index_of(a, e):
    try:
        return a.index(e)
    except ValueError:
        return -1

def is_assign(t):
    i = index_of(assign, str(t))
    if (i == -1): return None
    return assign[i]

def is_alpha(s):
    s = str(s)
    if index_of(keywords, s) >= 0:
        return False
    if len(s) > 0:  # Ensure the string is not empty
        return s[0].isalpha()
    return False  # Return False for an empty string

# creates enodes for module, translating methods after reading classes and imports
# this is the code for parsing enodes, but not converting them to C99
# that is in transpile

def etype(n):
    return n.type if isinstance(n, ENode) else n

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

    def parse_type_tokens():
        tokens = []
        while True:
            t = next_token()
            assert is_alpha(t.value), "enodes / parse_type / is_alpha"
            tokens.append(t)
            p = peek_token()
            if not p.value == '::': break
        return tokens
    
    # its useful that types are never embedded in other types. theres no reason for extra complexity
    def parse_member_tokens():
        tokens = []
        while True:
            t = next_token()
            assert is_alpha(t.value), "enodes / parse_type / is_alpha"
            tokens.append(str(t))
            p = peek_token()
            if not p.value == '.': break
        return tokens
    
    def peek_member_path():
        tokens = []
        ahead = 0
        while True:
            t = peek_token(ahead)
            if not is_alpha(t.value):
                tokens = []
                break
            assert is_alpha(t.value), "enodes / parse_type / is_alpha"
            tokens.append(str(t))
            p = peek_token(ahead + 1)
            if not p or not p.value == '.': break
            ahead += 2
        return tokens
    
    def parse_member_tokens_2():
        tokens = []
        ctx = None
        while True:
            t = next_token()
            if ctx == None:
                member = member_lookup()
                    
            assert is_alpha(t.value), "enodes / parse_type / is_alpha"
            tokens.append(str(t))
            p = peek_token()
            if not p.value == '.': break
        return tokens
    
    # used by the primary parsing mechanism
    def peek_type_tokens():
        tokens = []
        ahead = 0
        while True:
            t = peek_token(ahead)
            if not is_alpha(t.value):
                tokens = []
                break
            assert is_alpha(t.value), "enodes / parse_type / is_alpha"
            tokens.append(t)
            p = peek_token(ahead + 1)
            if not p or not p.value == '::': break
            ahead += 2
        return tokens
    
    # this wont actually validate the meta ordering which resolve_type subsequently performs
    def is_type(tokens:List[Token]):
        return tokens and str(tokens[0]) in module.defs
    
    # lookup access to member
    def lookup_member(s:str):
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
            cl = cl.module.find_class(cl.inherits)
        return None, None
    
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
                assert t and m, "member lookup failed on %s.%s" % (t.definition.name, s)
            return t, m
        return None, None

    # instance cached EType from tokens
    def resolve_type(tokens:List[Token]):   # example: array::num
        cursor = 0
        remain = len(tokens)

        def pull_token():
            nonlocal cursor, remain
            assert cursor < len(tokens)
            remain -= 1
            res = tokens[cursor]
            cursor += 1
            return res

        def resolve() -> Tuple[EType, str]:
            nonlocal remain
            key_name   = ''                             # initialize key as empty (sets to the value given by user)
            class_name = pull_token()                   # read token
            class_def  = module.find_class(class_name)  # lookup type in our module
            assert class_def != None                    # its a compilation error if it does not exist

            meta_len = len(class_def.meta_types)        # get length of meta types on the class def
            assert meta_len <= remain, "meta types mismatch"

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
            type = EType(definition=class_def, meta_types=meta_types)
            class_def.module.type_cache[key_name] = type
            return type, key_name

        return resolve()
    
    def parse_expression():
        return parse_add()
    
    def consume():  next_token()

    def is_primitive(enode):
        type = etype(enode)
        return type.model != 'allocated' and type.model.size > 0
    
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
            assert op_type0 in operators, "operator0 not found"
            assert op_type1 in operators, "operator1 not found"
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

    def parse_args():
        assertion(peek_token() == "[", "expected [ for args")
        consume()
        enode_args = []
        while (1):
            op = parse_expression()
            enode_args.append(op)
            if peek_token() == ",":
                consume()
            else:
                break
        assertion(peek_token() == "]", "expected ] after args")
        consume()
        return enode_args
        
    def type_of(value):
        tokens = None
        if isinstance(value, ENode):
            return value.type
        if isinstance(value, str):
            tokens = [Token(value=value, line_num=0)]
        assert tokens, "not implemented"
        type, type_s = resolve_type(tokens)
        return type

    def parse_primary():
        id = peek_token() # may have to peek the entire tokens set for member path
        if not id:
            id = peek_token()
        print("parse_primary: %s" % (id.value))

        if id == '[':
            consume()
            cast_expr = parse_expression()
            consume() # consume end bracket
            assert peek_token() == ']', "expected closing parenthesis"
            if isinstance(cast_expr, EType):
                expr   = parse_expression()
                method = castable(expr, cast_expr)
                return EMethodCall(type=etype(cast_expr), target=expr, method=method, args=[])
            else:
                return EParenthesis(type=etype(cast_expr), enode=cast_expr) # not a cast expression, just an expression
    
        n = is_numeric(id)
        if n != EUndefined:
            f = peek_token()
            consume()
            type = type_of('f64' if n == ELiteralReal else 'i64')
            return n(type=type, value=value_for_type(type=n, token=f))
        
        s = is_string(id)
        if s != EUndefined:
            f = peek_token()
            consume()
            return s(type=type_of('string'), value=f.value) # remove the quotes
        
        # we may attempt to peek at an entire type signature (will still need to peek against member access, too)
        # contructors require args.. thats why you define them, to construct 'with' things
        # otherwise one can override init; the edge case is doing both, in which case we resolve that issue in code
        type_tokens = peek_type_tokens()
        len_type_tokens = len(type_tokens) * 2 - 1
        if is_type(type_tokens):
            type = resolve_type(type_tokens)
            if peek_token(len_type_tokens + 1) == '[':
                # construction
                enode_args = parse_args()
                conv = enode_args
                method = None
                # if args are given, we use a type-matched constructor (first arg is primary)
                if len(enode_args):
                    method = constructable(enode_args[0], type)
                    assert method, "cannot construct with type %s" % enode_args[0].type.definition.name
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
                return EMethodCall(type=type, target=imember, method=member, args=conv_args)
        
        # read member stack
        i = is_reference(id) # line: 1085 in example.py: this is where its wrong -- n is defined as a u32 but its not returning reference here
        if i != EUndefined:
            f = peek_token()
            consume()
            return i

        return None
    
    def reset_member_depth():
        nonlocal member_stack
        member_stack = [OrderedDict()]
    
    def push_member_depth():
        nonlocal member_stack
        member_stack.append(OrderedDict())

    def pop_member_depth():
        return member_stack.pop()

    def push_member(member:EMember, access_name:str = None):
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
        if fr == to:
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
            assert len(method.args), "constructor must have args"
            if method.args[0].type == to:
                return method
        return None

    def convertible(efrom, to):
        fr = etype(efrom)
        if fr == etype: return True
        return castable(fr, to) or constructable(fr, to)

    def convert_enode(enode, type):
        assert enode.type, "code has no type. thats trash code"
        if enode.type != type:
            cast_m      = castable(enode, type)
            if cast_m:      return EConstruct(type=type, method=cast_m,      args=[enode]) # convert args to List[ENode] (same as EMethod)
            construct_m = constructable(enode, type)
            if construct_m: return EConstruct(type=type, method=construct_m, args=[enode])
            assert False, 'type conversion not found'
        else:
            return enode

    def convert_args(method, args):
        conv = []
        len_args = len(args)
        len_method = len(method.args)
        assert len_args == len_method, "arg count mismatch %i (user) != %i (impl)" % (len_args, len_method)
        arg_keys = list(method.args.keys())
        for i in range(len_args):
            u_arg_enode = args[i]
            m_arg_member = method.args[arg_keys[i]]
            assert u_arg_enode.type, "user arg has no type"
            assert m_arg_member.type, "method arg has no type"
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
            type, s_type = resolve_type(type_tokens)
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
                    assert False, "compilation error: unassigned instance"
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
                    member = EMember(name=str(after_type), access=None, type=type, value=None, visibility='public')
                    consume()
                    after  = peek_token()
                    assign = is_assign(after)
                    push_member(member)
                    if assign:
                        consume()
                        return EAssign(type=member.type, target=member, value=parse_expression())
                    else:
                        return member

        elif is_alpha(t0): # and non-keyword, so this is a variable that must be in stack
            t1 = peek_token(1) # this will need to allow for class static
            assign = t1 == ':'
            if assign:
                next_token()  # Consume '='
                member = lookup_member(str(t0))
                assert member, "member lookup failed: %s" % (str(t0))
                return EAssign(target=member, value=parse_expression())
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
        
        elif t0 == "return":
            next_token()  # Consume 'return'
            result = parse_expression()
            return EMethodReturn(type=type_of(result), value=result)
        
        elif t0 == "break":
            next_token()  # Consume 'break'
            levels = None
            if peek_token() == "[":
                next_token()  # Consume '['
                levels = parse_expression()
                assert peek_token() == "]", "expected ']' after break[expression...]"
                next_token()  # Consume ']'
            return EBreak(type=None, levels=levels)
        
        elif t0 == "for":
            next_token()  # Consume 'for'
            assert peek_token() == "[", "expected condition expression '['"
            next_token()  # Consume '['
            push_member_depth()
            statement = parse_statements()
            assert next_token() == ";", "expected ';'"
            condition = parse_expression()
            assert next_token() == ";", "expected ';'"
            post_iteration = parse_expression()
            assert next_token() == "]", "expected ']'"
            # i believe C inserts another level here with for; which is probably best
            push_member_depth()
            for_block = parse_statements()
            pop_member_depth()
            pop_member_depth()
            return EFor(type=None, init=statement, condition=condition, update=post_iteration, body=for_block)
        
        elif t0 == "while":
            next_token()  # Consume 'while'
            assert peek_token() == "[", "expected condition expression '['"
            next_token()  # Consume '['
            condition = parse_expression()
            assert next_token() == "]", "expected condition expression ']'"
            push_member_depth()
            statements = parse_statements()
            pop_member_depth()
            return EWhile(type=None, condition=condition, body=statements)
        
        elif t0 == "if":
            # with this member space we can declare inside the if as a variable which casts to boolean
            push_member_depth()
            next_token()  # Consume 'if'
            assert peek_token() == "[", "expected condition expression '['"
            next_token()  # Consume '['
            condition = parse_expression()
            assert next_token() == "]", "expected condition expression ']'"
            push_member_depth()
            statements = parse_statements()
            pop_member_depth()
            else_statements = None
            if peek_token() == "else":
                next_token()  # Consume 'else'
                push_member_depth()
                if peek_token() == "if":
                    next_token()  # Consume 'else if'
                    else_statements = parse_statements()
                else:
                    else_statements = parse_statements()
                pop_member_depth()
            pop_member_depth()
            return EIf(type=None, condition=condition, body=statements, else_body=else_statements)
        
        elif t0 == "do":
            next_token()  # Consume 'do'
            push_member_depth()
            statements = parse_statements()
            pop_member_depth()
            assert next_token() == "while", "expected 'while'"
            assert peek_token() == "[", "expected condition expression '['"
            next_token()  # Consume '['
            condition = parse_expression()
            assert next_token() == "]", "expected condition expression ']'"
            return EDoWhile(type=None, condition=condition, body=statements)
        
        else:
            return parse_expression()

    def parse_statements():
        block = []  # List to hold enode instances
        multiple = peek_token() == "["

        if multiple:
            next_token()  # Consume '['

        depth = 0
        while peek_token():
            t = peek_token()
            if t == '[':
                depth += 1
                push_member_depth()
                consume()
                continue
            if t == 'n':
                test = 1
            n = parse_statement()  # Parse the next statement
            assert n is not None, "expected statement or expression"
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

    def finish_method(cl, token, is_ctr):
        prev_token()
        return_type = parse_type_tokens() # read the whole type!
        name_token = next_token() if not is_ctr else None
        assert next_token().value == '[', f"Expected '[', got {token.value} at line {token.line_num}"
        
        # parse method args
        arg_type_token = peek_type_tokens()
        arg_type_token0 = arg_type_token
        args:OrderedDict[str, EProp] = OrderedDict()
        if not arg_type_token:
            t = peek_token()
            assert t == ']', 'expected ]'
            consume()
        else:
            while arg_type_token:
                arg_type_token = parse_type_tokens()
                assert arg_type_token, "failed to parse type in arg"
                arg_name_token = next_token()
                arg_name = str(arg_name_token)
                assert is_alpha(arg_name_token), "arg-name (%s) read is not an identifier" % (arg_name_token)
                arg_type, s_arg_type = resolve_type(arg_type_token)
                assert arg_type, "arg could not resolve type: %s" % (arg_type_token[0].value)
                args[arg_name] = EProp(name=str(arg_name_token), access=None, type=arg_type, value=None, visibility='public')
                if peek_token() == ',':
                    next_token()  # Consume ','
                else:
                    nx = peek_token()
                    assert nx.value == ']'
                if peek_token() == ']':
                    consume()
                    break

        t = peek_token()
        assert t == '[', f"Expected '[', got {token.value} at line {token.line_num}"
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
        rtype, s_rtype = resolve_type(return_type)
        id = 'construct' if is_ctr else str(name_token)
        method = EMethod(
            name=id, type=rtype, value=None, access='self', 
            args=args, body=body, visibility='public')

        if not id in cl.members:
            cl.members[id] = [method] # we may also retain the with_%s pattern in A-Type
        else:
            cl.members[id].append(method)

        return method

    def finish_class(cl):
        if not cl.block_tokens: return
        push_token_state(cl.block_tokens)
        token = peek_token()
        while (token := next_token()).value != ']':
            if token.value in ('public', 'intern'):
                visibility  = token.value
                type_tokens = parse_type_tokens()
                type, s_type = resolve_type(tokens=type_tokens)
                name_token  = next_token()
                next_token_value = peek_token().value
                if next_token_value == ':':
                    next_token()  # Consume the ':'
                    value_token = next_token()
                    prop_node = EProp(
                        type=type, name=name_token.value, access='self',
                        value=value_token.value, visibility=visibility)
                else:
                    prop_node = EProp(
                        type=type, name=name_token.value, access='self',
                        value=None, visibility=visibility)
                id = str(name_token)
                assert not id in cl.members, 'member exists' # we require EMethod != EProp checks too
                cl.members[id] = [prop_node]
            elif token == cl.name and peek_token(1) == '[':
                finish_method(cl, token, True)
            elif is_type([token]):
                finish_method(cl, token, False)
        pop_token_state()

    def parse_class():
        token = next_token()
        assert token == 'class', f"Expected 'class', got {token.value} at line {token.line_num}"
        name_token = next_token()
        assert name_token is not None
        class_node = EClass(module=module, name=name_token.value)

        # set 'model' of class -- this is the storage model, with default being an allocation (heap)
        # model handles storage and primitive logic
        # for now only allocated classes have 
        read_block = True
        if peek_token() == '::':
            consume()
            model_type = str(next_token())
            assert model_type in models, "model type not found: %s" % (model_type)
            class_node.model = models[model_type]
            read_block = model_type == 'allocated'
        else:
            class_node.model = models['allocated']

        if peek_token() == ':':
            consume()
            class_node.inherits = module.find_class(next_token())
        
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

        for name, a_members in class_def.members.items():
            for member in a_members:
                push_member(member, 'self') # if context name not given, we will perform memory management (for args in our case)
        push_member_depth()
        for name, member in method.args.items():
            push_member(member)
        # we need to push the args in, as well as all class members (prior, so args are accessed first)
        enode = parse_statements()
        pop_token_state()
        print('printing enodes for method %s' % (method.name))
        print_enodes(enode)
        return enode

    # top level module parsing
    while index < len(tokens):
        token = next_token()
        if token.value == 'class':
            index -= 1  # Step back to let parse_class handle 'class'
            class_node = parse_class()
            assert not class_node.name in module.defs, "%s duplicate definition" % (class_node.name)
            module.defs[class_node.name] = class_node

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
        else:
            assert False

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

def change_ext(f: str, ext_to: str) -> str:
    base, _ = os.path.splitext(f)
    return f"{base}.{ext_to.lstrip('.')}"

def header_emit(self, h_module_file = None):
    # write includes, for each import of silver module (and others that require headers)
    file = open(h_module_file, 'w')
    file.write('/* generated silver C99 emission for module: %s */\n' % (self.name))
    file.write('#include <A>\n')
    for name, i in self.imports.items():
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
                        file.write('\ti_public(X,Y,Z, %s, %s)\\\n' % (member.type.definition.name, member.name))
            for member_name, member in cl.members.items():
                for member in a_members:
                    if isinstance(member, EMethod):
                        arg_types = ''
                        for arg_name, a in member.args.items():
                            arg_types += ', %s' % str(a.type.definition.name) # will need a suffix on extra member functions (those defined after the first will get _2, _3, _4)
                        file.write('\ti_method(X,Y,Z, %s, %s%s)\\\n' % (member.type.definition.name, member.name, arg_types))
            file.write('\n')
            if cl.inherits:
                file.write('declare_mod(%s, %s)\n' % (cl.name, cl.inherits))
            else:
                file.write('declare_class(%s)\n' % (cl.name))

def replace_nodes(enode, replacement_func):
    # Check if the current node should be replaced
    replacement = replacement_func(enode)
    if replacement is not None:
        return replacement

    return enode

@dataclass
class enode_emit:
    module:EModule
    method:EMethod
    #converted:OrderedDict[ENode, ENode] = field(default_factory=OrderedDict)
    # EMethodCall -> EReference; method call restructuring to make statements C99 emittable.
    # lets just change the enode members to EReference after processing EMethodCall
    #def lookup(self, n:ENode):
    #    if n in self.converted:
    #        return self.converted[n]
    #    else:
    #        return n
    
    def emit(self, n:ENode):
        t = type(n)
        method = f"emit_{t.__name__}"
        if hasattr(self, method): return getattr(self, method)(n)
        raise Exception('not implemented: %s' % (t.__name__))
    

    def emit_expr(self, enode:ENode, member_temp:EMember = None):
        '''
        method_calls = enode.each(EMethodCall)
        if len(method_calls):

            # for each method call
            for method_call in method_calls:

                # variable for return result
                res += '%s _result_%i;\n' % (method_call.method.type.definition.name, result_count)
                result_count += 1

                # guard namespace for args, and invoke inside
                method_call.arg_temp_members = []
                arg_number = 0

                # go through args and emit expressions with recursion into emit_expr
                # temporary arg var emitted here, which we will set return values into
                for arg_enode in method_call.args:
                    # simply need the type from this arg (enabled w convert_args)
                    assert(arg_enode.type)
                    member_arg = EMember(name='_arg_%i' % arg_number, type=arg_enode.type, access=None, value=None, visibility='public')
                    res += '%s %s;\n' % (member.type.definition.name, member.name)

                    # now we assign this value by emitting the expression
                    # notice the recursion; considered a list of statements to obtain the value we use for the arg
                    res += self.emit_expr(arg_enode, member_arg)
                    method_call.arg_temp_members.append(member_arg)
                    arg_number += 1

        # for each EMethodCall in method_calls
        if member_temp:
            res += '{\n %s = ' # needs to be isolated operation when member_temp is set
        '''
        res = self.emit(enode)
        return res
    
    def emit_EMul(self, n:EMul): return self.emit(n.left) + ' * ' + self.emit(n.right)
    def emit_EDiv(self, n:EDiv): return self.emit(n.left) + ' / ' + self.emit(n.right)
    def emit_EAdd(self, n:EAdd): return self.emit(n.left) + ' + ' + self.emit(n.right)
    def emit_ESub(self, n:ESub): return self.emit(n.left) + ' - ' + self.emit(n.right)

    def emit_ELiteralInt(self,  n:ELiteralInt):  return str(n.value)
    def emit_ELiteralReal(self, n:ELiteralReal): return repr(n.value)

    # these need to support class membership (may be useful to have another type)
    def emit_EProp(self, n:EProp):   return '%s->%s' % (n.access, n.name) if n.access else n.name
    def emit_EMember(self, n:EProp): return '%s->%s' % (n.access, n.name) if n.access else n.name

    def emit_EStatements(self, n:EStatements):
        res = ''
        for enode in n.value:
            res += self.emit(enode)
        return res
    
    def emit_EAssign(self, n:EAssign):
        pass

    def emit_EMethodReturn(self, n:EMethodReturn):
        #result_member = EMember(name='_result_', type=self.method.type, access=None, value=None, visibility='public')
        #res = '%s %s;\n' % (self.method.type.definition.name, result_member.name)
        #res += self.emit(n.value, result_member)
        #return 'return %s;\n' % self.method.type.definition.name
        return 'return %s\n' % self.emit(n.value)

    def emit_EMethodCall(self, n:EMethodCall):
        pass

def module_emit(self, c_module_file = None):
    if not c_module_file: c_module_file = self.name + '.c'
    assert index_of(c_module_file, '.c') >= 0, 'can only emit .c (C99) source.'
    print('emitting to %s/%s' % (os.getcwd(), c_module_file))
    file = open(c_module_file, 'w')
    header = change_ext(c_module_file, "h")
    header_emit(self, header)
    
    file.write('#include <%s.h>\n\n' % (self.name))
    for name, cl in self.defs.items():
        if isinstance(cl, EClass):
            # only emit silver-based implementation
            if cl.model.name != 'allocated': continue
            for member_name, a_members in cl.members.items():
                for member in a_members:
                    if isinstance(member, EMethod):
                        args = ''
                        for arg_name, a in member.args.items():
                            args += ', %s %s' % (
                                a.type.definition.name, a.name)
                        file.write('%s %s_%s(%s self%s) {\n' % (
                            member.type.definition.name, cl.name, member.name, cl.name, args))
                        file.write('\t/* todo */\n')
                        enode_emitter = enode_emit(self, member)
                        file.write(enode_emitter.emit(member.code)) # method code should first contain references to the args
                        file.write('}\n')
            file.write('\n')
            if cl.inherits:
                file.write('define_mod(%s, %s)\n' % (cl.name, cl.inherits))
            else:
                file.write('define_class(%s)\n' % (cl.name))
    return

# all of the basic types come directly from A-type implementation
# string, array, map, hash-map etc

# example usage:
# i need to simply emit these into class-defs in python
# no use exposing model, the user may simply use one of the primitives
input = """

class app [
    public int arg: 0
    public string another
    intern string hidden: 'dragon'

    # called by run
    num something[ num x, real mag ] [ return x * 1 * mag ]

    # app entrance method; one could override init as well
    num run[] [
        u8 i: 2 * arg
        u16 a: 2
        u32 n: 1
        return n + something[ a * i, 2 ]
    ]
]
"""

# silver44 (0.4.4) is python, but 1.0.0 will be C

t = tokens(input)
m = EModule(name='example')

# define primitives first
m.defs['bool'] = EClass(module=m, name='bool', model=models['boolean-32'])
m.defs['u8']   = EClass(module=m, name='u8',   model=models['unsigned-8'])
m.defs['u16']  = EClass(module=m, name='u16',  model=models['unsigned-16'])
m.defs['u32']  = EClass(module=m, name='u32',  model=models['unsigned-32'])
m.defs['u64']  = EClass(module=m, name='u64',  model=models['unsigned-64'])
m.defs['i8']   = EClass(module=m, name='i8',   model=models['signed-8'])
m.defs['i16']  = EClass(module=m, name='i16',  model=models['signed-16'])
m.defs['i32']  = EClass(module=m, name='i32',  model=models['signed-32'])
m.defs['i64']  = EClass(module=m, name='i64',  model=models['signed-64'])
m.defs['f32']  = EClass(module=m, name='f32',  model=models['real-32'])
m.defs['f64']  = EClass(module=m, name='f64',  model=models['real-64'])
m.defs['int']  = EClass(module=m, name='i32',  model=models['signed-32'])
m.defs['num']  = EClass(module=m, name='i64',  model=models['signed-64'])
m.defs['real'] = EClass(module=m, name='real', model=models['real-64'])

# define reference classes from AType
# use reflection for defining props, methods & args  
m.defs['string'] = EClass(module=m, name='string', model=models['atype'])

m.parse(t)
print('-----------------------')
app = m.defs['app']
for name, a_members in app.members.items():
    for member in a_members:
        if isinstance(member, EMethod):
            print('method: %s' % (name))
            print_enodes(node=member.code, indent=0)

m.emit()

