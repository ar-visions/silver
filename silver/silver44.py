from dataclasses import dataclass, field
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

@dataclass
class ENode:
    pass

@dataclass
class EModule(ENode):
    name:       str
    path:       Path = None
    imports:    OrderedDict[str, 'ENode'] = field(default_factory=OrderedDict)
    defs:       OrderedDict[str, 'ENode'] = field(default_factory=OrderedDict)
    type_cache: OrderedDict[str, 'EType'] = field(default_factory=OrderedDict)
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

@dataclass
class EAccess:
    member: 'EMember'
    access_name: str
    def __repr__(self): return f"EAccess(member={self.member}, access_name={self.access_name})"

@dataclass
class EModel:
    name:str
    size:int
    integral:bool
    type:Type

@dataclass
class HeapType:
    placeholder: bool

@dataclass
class AType:
    placeholder: bool

models:OrderedDict[str, EModel] = OrderedDict()
models['atype']       = EModel(name='atype',       size=8, integral=0, type=AType)
models['allocated']   = EModel(name='allocated',   size=8, integral=0, type=HeapType)
models['boolean-32']  = EModel(name='boolean_32',  size=4, integral=1, type=bool)
models['unsigned-8']  = EModel(name='unsigned_8',  size=1, integral=1, type=np.uint8)
models['unsigned-16'] = EModel(name='unsigned_16', size=2, integral=1, type=np.uint16)
models['unsigned-32'] = EModel(name='unsigned_32', size=4, integral=1, type=np.uint32)
models['unsigned-64'] = EModel(name='unsigned_64', size=8, integral=1, type=np.uint64)
models['signed-8']    = EModel(name='signed_8',    size=1, integral=1, type=np.int8)
models['signed-16']   = EModel(name='signed_16',   size=2, integral=1, type=np.int16)
models['signed-32']   = EModel(name='signed_32',   size=4, integral=1, type=np.int32)
models['signed-64']   = EModel(name='signed_64',   size=8, integral=1, type=np.int64)
models['real-32']     = EModel(name='real_32',     size=4, integral=0, type=np.float32)
models['real-64']     = EModel(name='real_64',     size=8, integral=0, type=np.float64)

@dataclass
class EClass(ENode):
    module: EModule
    name: str
    model: EModel = None
    inherits: 'EClass' = None
    block_tokens: List[Token] = field(default_factory=list)
    meta_types:List[ENode] = field(default_factory=list)
    members:OrderedDict[str, 'EMember'] = field(default_factory=OrderedDict)
    def __repr__(self): return f"EClass(name={self.name})"

@dataclass
class EType(ENode):
    definition: EClass # eclass is resolved here, and we form
    meta_types: List['EType'] = field(default_factory=list)
    def __repr__(self): return f"EType(definition={self.definition}, meta_types={self.meta_types})"

@dataclass
class EMember(ENode):
    name: str
    type: EType
    value: ENode
    visibility: str
    def __repr__(self): return f"EProp(name={self.name}, type={self.type}, value={self.value}, visibility={self.visibility})"

# EProp will have an initializer ENode
@dataclass
class EMethod(EMember):
    body:List[Token]
    args:OrderedDict[str,EMember]
    code:ENode = None
    def __repr__(self): return f"EMethod(name={self.name}, type={self.type}, args={self.args})"

@dataclass
class EConstruct(ENode):
    type: EType
    args:OrderedDict[str,'EProp']
    def __repr__(self): return f"EConstruct(args={self.args}, type={self.type})"

@dataclass
class EProp(EMember):
    def __repr__(self):
        return f"EProp(name={self.name}, type={self.type}, value={self.value}, visibility={self.visibility})"

@dataclass
class EUndefined(ENode):
    def __repr__(self): return "EUndefined()"

@dataclass
class EDeclaration(ENode):
    member:EMember
    def __repr__(self): return f"EDeclaration(type={self.type}, name={self.name})"

@dataclass
class EAssign(ENode):
    target:ENode
    value:ENode
    declaration:EDeclaration = None
    def __repr__(self): return f"EAssign(target={self.target}, value={self.value})"

@dataclass
class EAssignAdd(ENode):
    target:ENode
    value:ENode
    def __repr__(self): return f"EAssignAdd(target={self.target}, value={self.value})"

@dataclass
class EAssignSub(ENode):
    target:ENode
    value:ENode
    def __repr__(self): return f"EAssignSub(target={self.target}, value={self.value})"

@dataclass
class EAssignMul(ENode):
    target:ENode
    value:ENode
    def __repr__(self): return f"EAssignMul(target={self.target}, value={self.value})"

@dataclass
class EAssignDiv(ENode):
    target:ENode
    value:ENode
    def __repr__(self): return f"EAssignDiv(target={self.target}, value={self.value})"

@dataclass
class EAssignOr(ENode):
    target:ENode
    value:ENode
    def __repr__(self): return f"EAssignOr(target={self.target}, value={self.value})"

@dataclass
class EAssignAnd(ENode):
    target:ENode
    value:ENode
    def __repr__(self): return f"EAssignAnd(target={self.target}, value={self.value})"

@dataclass
class EAssignXor(ENode):
    target:ENode
    value:ENode
    def __repr__(self): return f"EAssignXor(target={self.target}, value={self.value})"

@dataclass
class EAssignShiftR(ENode):
    target:ENode
    value:ENode
    def __repr__(self): return f"EAssignShiftR(target={self.target}, value={self.value})"

@dataclass
class EAssignShiftL(ENode):
    target:ENode
    value:ENode
    def __repr__(self): return f"EAssignShiftL(target={self.target}, value={self.value})"

@dataclass
class EAssignMod(ENode):
    target:ENode
    value:ENode
    def __repr__(self): return f"EAssignMod(target={self.target}, value={self.value})"

@dataclass
class EIf(ENode):
    condition:ENode
    body:List[Token]
    else_body:ENode
    def __repr__(self): return f"EIf(condition={self.condition}, body={self.body}, else_body={self.else_body})"

@dataclass
class EFor(ENode):
    init:ENode
    condition:ENode
    update:ENode
    body:ENode
    def __repr__(self): return f"EFor(init={self.init}, condition={self.condition}, update={self.update}, body={self.body})"

@dataclass
class EWhile(ENode):
    condition:ENode
    body:ENode
    def __repr__(self): return f"EWhile(condition={self.condition}, body={self.body})"

@dataclass
class EDoWhile(ENode):
    condition:ENode
    body:ENode
    def __repr__(self): return f"EDoWhile(condition={self.condition}, body={self.body})"

@dataclass
class EBreak(ENode):
    def __repr__(self): return "EBreak()"

@dataclass
class ELiteralReal(ENode):
    value:float
    def __repr__(self): return f"ELiteralReal(value={self.value})"

@dataclass
class ELiteralInt(ENode):
    value:int
    def __repr__(self): return f"ELiteralInt(value={self.value})"

@dataclass
class ELiteralStr(ENode):
    value:str
    def __repr__(self): return f"ELiteralStr(value={self.value})"

def value_for_type(type, token):
    if type == ELiteralStr:  return token.value # token has the string in it
    if type == ELiteralReal: return float(token.value)
    if type == ELiteralInt:  return int(token.value)

@dataclass
class ELiteralStrInterp(ENode):
    value:str
    # we may want a kind of context here
    def __repr__(self): return f"ELiteralStrInterp(value={self.value})"

@dataclass
class EArray(ENode):
    elements:List[ENode]
    def __repr__(self): return f"EArray(elements={self.elements})"

@dataclass
class EAlphaIdent(ENode):
    name:Token
    def __repr__(self): return f"EAlphaIdent(name={self.name})"

@dataclass
class EReference(ENode):
    member:EMember
    def __repr__(self): return f"EReference(member={self.member})"

@dataclass
class EAdd(ENode):
    left:ENode
    right:ENode
    def __repr__(self): return f"EAdd(left={self.left}, right={self.right})"

@dataclass
class ESub(ENode):
    left:ENode
    right:ENode
    def __repr__(self):
        return f"ESub(left={self.left}, right={self.right})"

@dataclass
class EMul(ENode):
    left:ENode
    right:ENode
    def __repr__(self):
        return f"EMul(left={self.left}, right={self.right})"

@dataclass
class EDiv(ENode):
    left:ENode
    right:ENode
    def __repr__(self): return f"EDiv(left={self.left}, right={self.right})"

@dataclass
class EOr(ENode):
    left:ENode
    right:ENode
    def __repr__(self): return f"EOr(left={self.left}, right={self.right})"

@dataclass
class EAnd(ENode):
    left:ENode
    right:ENode
    def __repr__(self): return f"EAnd(left={self.left}, right={self.right})"

@dataclass
class EXor(ENode):
    left:ENode
    right:ENode
    def __repr__(self): return f"EXor(left={self.left}, right={self.right})"

@dataclass
class EMethodCall(ENode):
    method:ENode
    args:List[ENode]
    def __repr__(self): return f"EMethodCall(method={self.method}, args={self.args})"

@dataclass
class EMethodReturn(ENode):
    value:ENode
    def __repr__(self): return f"EMethodReturn(value={self.value})"

@dataclass
class EStatements(ENode):
    value:List[ENode]
    def __repr__(self): return f"EStatements(value={self.value})"

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

def parse_tokens(*, module:EModule, _tokens:List['Token']):
    index = 0
    tokens = _tokens
    token_bank = []
    member_stack:List[OrderedDict[str, EAccess]] = []  # we must push to this for available members in class, lambda, etc

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
    def lookup_access(s:str):
        for i in range(len(member_stack)):
            index = len(member_stack) - 1 - i
            map = member_stack[index]
            if s in map:
                access = map[s]
                return access
        return None
    
    def member_lookup(cl:EType, name:str) -> Tuple[EType, EMember]:
        while True:
            cl = t.definition
            if name in t.members:
                return t, cl.members[name]
            if not cl.inherits:
                break
            cl = cl.module.find_class(cl.inherits)
        return None, None
    
    def resolve_member(member_path:List[str]): # example object-var-name.something-inside
        # we want to output the EType, and its EMember
        f = lookup_access(member_path[0])
        if f:
            member_type = f.type
            # access_name = f.access_name
            t = f.type
            m = None
            for token in member_path[1:]:
                s = str(token)
                t, m = member_lookup(t, s)
                assert t and m, "member lookup failed on %s.%s" % (t.definition.name, s)
            

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

    def parse_add():
        left = parse_mult()
        while peek_token() == "+" or peek_token() == "-":
            etype = EAdd if peek_token() == "+" else ESub
            consume()
            right = parse_mult()
            left = etype(left, right)
        return left
    
    def parse_mult():
        left = parse_primary()
        while peek_token() == "*" or peek_token() == "/":
            etype = EMul if peek_token() == "*" else EDiv
            consume()
            right = parse_primary()
            left = etype(left, right)
        return left
    
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
                return EReference(member=member)
        return EUndefined

    def parse_primary():
        id = peek_token() # may have to peek the entire tokens set for member path
        print("parse_primary: %s" % (id.value));
        n = is_numeric(id)
        if n != EUndefined:
            f = peek_token()
            consume()
            return n(value=value_for_type(type=n, token=f))
        
        s = is_string(id)
        if s != EUndefined:
            f = peek_token()
            consume()
            return s(value=f.value) # remove the quotes
        
        # we may attempt to peek at an entire type signature (will still need to peek against member access, too)
        type_tokens = peek_type_tokens()
        len_type_tokens = len(type_tokens) * 2 - 1
        if is_type(type_tokens):
            type = resolve_type(type_tokens)
            if peek_token(len_type_tokens + 1) == '[':
                # construction
                consume()
                enode_args = []
                while (1):
                    op = parse_expression()
                    enode_args.append(op)
                    if peek_token() == ",":
                        consume()
                    else:
                        break
                
                assertion(peek_token() == "]", "expected ] after construction");
                consume()
                
                return EConstruct(type=type, args=enode_args)
            else:
                return type
        else:
            member_path = peek_member_path()
            len_member_tokens = len(member_path) * 2 - 1
            # if method call
            if len_member_tokens > 0 and peek_token(len_member_tokens) == '[':
                for i in range(1 + len_member_tokens):
                    consume()
                t0 = peek_token()
                enode_args = []
                while (1):
                    op = parse_expression()
                    enode_args.append(op)
                    if peek_token() == ",":
                        consume()
                    else:
                        break
                t = peek_token()
                assertion(t == "]", "expected ] after method invocation");
                consume()
                return EMethodCall(member_path, enode_args)
        
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

    def push_member(m:EMember, access_name:str = None):
        nonlocal member_stack
        assert(m.name not in member_stack[-1])
        member_stack[-1][m.name] = EAccess(member=m, access_name=access_name)

    def parse_statement():
        t0 = peek_token()

        # when you cant think of the whole parser you just do a part of it
        # 'this' could be defined in this var-space 
        # emitting it in initializer function handles the implementation side
        # something.another: convertible[arg:1, arg2:convertible[]]
        
        # if type is found
        if is_type([t0]):
            type_tokens = parse_type_tokens()
            type = resolve_type(type_tokens)
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
                    # construct anonymous instance
                    args = OrderedDict()
                    # constructor args must be matched
                    # we should handle both named args and non-named
                    # should have everything we need to perform argument matching here
                    # this is not done yet because i have the dumb
                    return EAssign(target=None, value=EConstruct(type=type, args=args))
                else:
                    # call method
                    method = module.lookup_member(s_member_path)
                    return EMethodCall(method=method, args=args)
                #
                assert False
                return EMethodCall()
            else:
                if is_alpha(after_type):
                    member = EMember(name=str(after_type), type=type, value=None, visibility='public')
                    decl   = EDeclaration(member=member)
                    consume()
                    after  = peek_token()
                    assign = is_assign(after)
                    push_member(member)
                    if assign:
                        consume()
                        return EAssign(declaration=decl, target=after_type, value=parse_expression())
                    else:
                        return decl

        elif is_alpha(t0): # and non-keyword, so this is a variable that must be in stack
            #next_token()
            t1 = peek_token(1)
            assign = t1 == ':'
            if assign:
                next_token()  # Consume '='
                return EAssign(target=t0, value=parse_expression())
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
            return EMethodReturn(result)
        
        elif t0 == "break":
            next_token()  # Consume 'break'
            levels = None
            if peek_token() == "[":
                next_token()  # Consume '['
                levels = parse_expression()
                assert peek_token() == "]", "expected ']' after break[expression...]"
                next_token()  # Consume ']'
            return EBreak(levels=levels)
        
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
            return EFor(init=statement, condition=condition, update=post_iteration, body=for_block)
        
        elif t0 == "while":
            next_token()  # Consume 'while'
            assert peek_token() == "[", "expected condition expression '['"
            next_token()  # Consume '['
            condition = parse_expression()
            assert next_token() == "]", "expected condition expression ']'"
            push_member_depth()
            statements = parse_statements()
            pop_member_depth()
            return EWhile(condition=condition, body=statements)
        
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
            return EIf(condition=condition, body=statements, else_body=else_statements)
        
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
            return EDoWhile(condition=condition, body=statements)
        
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
        return EStatements(value=block)
    
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
                        type=type, name=name_token.value,
                        value=value_token.value, visibility=visibility)
                else:
                    prop_node = EProp(
                        type=type, name=name_token.value,
                        value=None, visibility=visibility)
                class_node.members[str(name_token)] = prop_node
            elif is_type([token]):
                prev_token()
                return_type = parse_type_tokens() # read the whole type!
                name_token = next_token()
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
                        args[arg_name] = EProp(name=str(arg_name_token), type=arg_type, value=None, visibility='public')
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
                    if body_token == ']': depth -= 1
                #else:
                #   body = parse_expression_tokens -- do this in 1.0, however its difficult to lazy load this without error
                rtype, s_rtype = resolve_type(return_type)
                class_node.members[str(name_token)] = EMethod(
                    name=str(name_token), type=rtype, value=None, 
                    args=args, body=body, visibility='public')
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

        for name, member in class_def.members.items():
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
            for name, member in enode.members.items():
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
        for name, member in node.members.items():
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
            for member_name, member in cl.members.items():
                if isinstance(member, EProp):
                    # todo: handle public / priv / intern
                    file.write('\ti_public(X,Y,Z, %s, %s)\\\n' % (member.type.definition.name, member.name))
            for member_name, member in cl.members.items():
                if isinstance(member, EMethod):
                    arg_types = ''
                    for arg_name, a in member.args.items():
                        arg_types += ', %s' % str(a.type.definition.name)
                    file.write('\ti_method(X,Y,Z, %s, %s%s)\\\n' % (member.type.definition.name, member.name, arg_types))
            file.write('\n')
            if cl.inherits:
                file.write('declare_mod(%s, %s)\n' % (cl.name, cl.inherits))
            else:
                file.write('declare_class(%s)\n' % (cl.name))

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
            for member_name, member in cl.members.items():
                if isinstance(member, EMethod):
                    args = ''
                    for arg_name, a in member.args.items():
                        args += ', %s %s' % (
                            a.type.definition.name, a.name)
                    file.write('%s %s_%s(%s self%s) {\n' % (
                        member.type.definition.name, cl.name, member.name, cl.name, args))
                    file.write('\t/* todo */\n')
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
for name, member in app.members.items():
    if isinstance(member, EMethod):
        print('method: %s' % (name))
        print_enodes(node=member.code, indent=0)

m.emit()

