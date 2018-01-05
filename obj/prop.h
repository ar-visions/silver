
forward List, Pairs, Enum;

class Prop {
    List props_with_meta(Class,const char *);
    C new_with(Class,char *, char *,Getter,Setter,char *);
    String name;
    Base value;
    Enum enum_type;
    Class class_type;
    Pairs meta;
    private Class prop_of;
    private Getter getter;
    private Setter setter;
};
