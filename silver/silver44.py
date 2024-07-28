class Token:
    def __init__(self, value, line_num):
        self.value = value
        self.line_num = line_num

    def __repr__(self):
        return f"Token({self.value}, line {self.line_num})"

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



class ASTNode:
    pass

class ClassNode(ASTNode):
    def __init__(self, name):
        self.name = name
        self.members = []

    def __repr__(self):
        return f"ClassNode(name={self.name})"

class MethodNode(ASTNode):
    def __init__(self, name, return_type):
        self.name = name
        self.return_type = return_type
        self.parameters = []
        self.body = []

    def __repr__(self):
        return f"MethodNode(name={self.name}, return_type={self.return_type})"

class PropertyNode(ASTNode):
    def __init__(self, name, type_, value=None, visibility='public'):
        self.name = name
        self.type = type_
        self.value = value
        self.visibility = visibility

    def __repr__(self):
        return f"PropertyNode(name={self.name}, type={self.type}, value={self.value}, visibility={self.visibility})"

class StatementNode(ASTNode):
    def __init__(self, statement):
        self.statement = statement

    def __repr__(self):
        return f"StatementNode(statement={self.statement})"



def create_ast(tokens):
    index = 0
    length = len(tokens)

    def next_token():
        nonlocal index
        if index < length:
            token = tokens[index]
            index += 1
            return token
        return None

    def peek_token():
        if index < length:
            return tokens[index]
        return None

    def parse_class():
        assert next_token().value == 'class', f"Expected 'class', got {token.value} at line {token.line_num}"
        name_token = next_token()
        assert name_token is not None
        class_node = ClassNode(name_token.value)
        assert next_token().value == '[', f"Expected '[', got {token.value} at line {token.line_num}"
        
        while (token := next_token()).value != ']':
            if token.value in ('public', 'intern'):
                visibility = token.value
                type_token = next_token()
                name_token = next_token()
                next_token_value = peek_token().value
                if next_token_value == ':':
                    next_token()  # Consume the ':'
                    value_token = next_token()
                    prop_node = PropertyNode(name_token.value, type_token.value, value_token.value, visibility)
                else:
                    prop_node = PropertyNode(name_token.value, type_token.value, visibility=visibility)
                class_node.members.append(prop_node)
            elif token.value in ('num', 'string', 'int'):
                return_type = token.value
                name_token = next_token()
                assert next_token().value == '[', f"Expected '[', got {token.value} at line {token.line_num}"
                method_node = MethodNode(name_token.value, return_type)
                
                # Parse parameters
                param_token = next_token()
                while param_token.value != ']':
                    method_node.parameters.append(param_token.value)
                    if peek_token().value == ',':
                        next_token()  # Consume ','
                    param_token = next_token()

                assert next_token().value == '[', f"Expected '[', got {token.value} at line {token.line_num}"
                
                # Parse method body
                body_token = next_token()
                while body_token.value != ']':
                    method_node.body.append(StatementNode(body_token.value))
                    body_token = next_token()
                
                class_node.members.append(method_node)
        
        return class_node

    ast = []
    while index < length:
        token = next_token()
        if token.value == 'class':
            index -= 1  # Step back to let parse_class handle 'class'
            class_node = parse_class()
            ast.append(class_node)

    return ast






def print_ast(node, indent=0):
    # handle [array]
    if isinstance(node, list):
        for n in node:
            print_ast(n, indent)
        return
    # handle specific AST Node class now:
    print(' ' * 4 * indent + str(node))
    if isinstance(node, ClassNode):
        for member in node.members:
            print_ast(member, indent + 1)
    elif isinstance(node, MethodNode):
        for param in node.parameters:
            print(' ' * 4 * (indent + 1) + f'Parameter: {param}')
        for body_stmt in node.body:
            print_ast(body_stmt, indent + 1)


# Example usage:
input_string = """
class app [
    public int arg: 0
    public string another
    intern string hidden: 'dragon'

    # called by run
    num something[ num x, real mag ] [ return x * 1 * mag ]

    # app entrance method; one could override init as well
    num run[] [
        num i: 2 + arg
        num a: 2
        num n: 1
        return n + something[ a + i, 2 ]
    ]
]
"""

t   = tokens(input_string)
#for token in t:
#    print(token)

ast = create_ast(t)
print_ast(ast)


