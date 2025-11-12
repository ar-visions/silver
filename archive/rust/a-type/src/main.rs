use paste::paste;
use std::ffi::c_void;
use std::ffi::c_char;

#[repr(u32)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum AMFlag {
    None                = 0,
    Construct           = 1,
    Prop                = 2,
    Inlay               = 4,
    Priv                = 8,
    Intern              = 16,
    ReadOnly            = 32,
    IMethod             = 64,
    SMethod             = 128,
    Operator            = 256,
    Cast                = 512,
    Index               = 1024,
    EnumV               = 2048,
    Override            = 4096,
    VProp               = 8192
}

#[repr(C, packed(1))]
pub struct meta_t {
    pub count:         i64,
    pub meta_0:        *mut f_A,
    pub meta_1:        *mut f_A,
    pub meta_2:        *mut f_A,
    pub meta_3:        *mut f_A,
    pub meta_4:        *mut f_A,
    pub meta_5:        *mut f_A,
    pub meta_6:        *mut f_A,
    pub meta_7:        *mut f_A,
    pub meta_8:        *mut f_A,
    pub meta_9:        *mut f_A,
}

#[repr(C, packed(1))]
pub struct member {
    pub name:           *mut c_char,
    pub _type_:         *mut c_void,
    pub offset:         i32,
    pub count:          i32,
    pub member_type:    i32,
    pub operator_type:  i32,
    pub required:       i32,
    pub args:           meta_t,
    pub ptr:            *mut c_void,
    pub method:         *mut c_void,
}

#[repr(C, packed(1))]
struct f_A {
    pub parent_type   : *mut f_A,
    pub name          : *mut c_char,
    pub module        : *mut c_char,
    pub size          : i32,
    pub msize         : i32,
    pub vmember_shape : shape,
    pub vmember_count : i32,
    pub vmember_type  : AMFlag,
    pub member_count  : i32,
    pub members       : *mut type_member_t,
    pub traits        : i32,
    pub src           : *mut f_A,
    pub arb           : *mut c_void,
    pub meta          : meta_t
}


#[repr(C, packed(1))]
struct A {
    pub _type_:         *mut f_A,
    pub _scalar_:       *mut f_A,
    pub refs:           i64,
    pub data:           *mut c_void,
    pub shape:          *mut VecI64,
    pub alloc:          i64,
    pub count:          i64,
}


#[macro_export]
macro_rules! A_type {
    (
        $name:ident {
            $(prop $prop_vis:vis $prop_name:ident: $prop_type:ty;)*
            $(method $method_vis:vis $method_name:ident($($arg:ident: $arg_type:ty),*) -> $ret:ty;)*
        }
    ) => {

        // attempted to define it first, then have this fill in more, but that hasn't worked out either
        // instance struct
        //#[repr(C, packed(1))]
        //pub struct $name {
        //    $( $prop_vis $prop_name: $prop_type ),*
        //}
        
        paste! {
            // f table struct 
            #[repr(C, packed(1))]
            pub struct [<f_ $name>] {
                // Base ftable_t fields
                pub parent_type:    *mut f_A,
                pub name:           *mut c_char,
                pub module:         *mut c_char,
                pub size:           i32,
                pub msize:          i32,
                pub vmember_shape:  *mut VecI64,
                pub vmember_count:  i32,
                pub vmember_type:   *mut f_A,
                pub member_count:   i32,
                pub members:        *mut type_member_t,
                pub traits:         i32,
                pub src:            *mut f_A,
                pub arb:            *mut c_void,
                pub meta:           meta_t,

                // Methods
                $($method_vis $method_name: extern "C" fn(*mut $name $(,$arg_type)*) -> $ret),*
            }
        }

        paste! {
            // type aliases matching C typedefs -- i wasn't sure if i needed two paste! instances for different joining
            pub type [<f_ $name>] = f_$name;
        }

        paste! {
            pub type [<ft_ $name>] = *mut f_$name;
        }
        
        paste! {
            #[allow(non_upper_case_globals)]
            pub static [<type_ $name>]: *mut f_A = unsafe { 
                $name::__register_type() 
            };
        }

        impl $name {
            pub unsafe fn new(
                    count:   i64,
                    af_pool: bool,
                    $($prop_name: Option<$prop_type>),*) -> *mut $name {
                let a_header_size = std::mem::size_of::<Au>();
                let is_base       = std::any::TypeId::of::<$name>() == std::any::TypeId::of::<obj>();
                let instance_size = std::mem::size_of::<$name>();
                let total_size    = a_header_size + if is_base { 0 } else { instance_size * count as usize };
                
                // allocate memory
                let ptr = std::alloc::alloc(
                    Layout::from_size_align(total_size, 1).unwrap()
                );
                
                // header and instance pointers
                let header   =  ptr as *mut Au;
                let instance = (ptr.add(header_size)) as *mut $name;

                // init object header
                paste! {
                    (*header)._type_    = & Self::[<type_ $name>];
                }
                (*header).refs      = if af_pool { 0 } else { 1 };
                (*header).data      = instance as *mut c_void;
                (*header).count     = count;
                (*header).alloc     = count;
                
                // ...if af_pool, register with pool...

                // emission for set properties
                $(
                    if let Some(val) = $prop_name {
                        std::ptr::write(&mut (*instance).$prop_name, val);
                    }
                )*
                instance
            }

            pub unsafe fn __register_type() -> *mut f_A {

                let base_type = &A_type; // Get active f_A instance

                paste! {
                    // Allocate and initialize the function table
                    let mut ftable = Box::new([<f_ $name>] {
                        parent_type:        std::ptr::null_mut(), // Will be set to A_type for base classes
                        name:               std::ffi::CString::new(stringify!($name)).unwrap().into_raw(),
                        module:             std::ptr::null_mut(), // Set by module registration
                        size:               std::mem::size_of::<$name>() as i32,
                        msize:              0, // For internal members
                        vmember_shape:      std::ptr::null_mut(),
                        vmember_count:      0,
                        vmember_type:       std::ptr::null_mut(),
                        member_count:       0,
                        members: {
                            let mut members = Vec::new();

                            // Copy base methods from f_A
                            for i in 0..(*base_type).member_count {
                                let base_member = &(*(*base_type).members.add(i as usize));
                                if base_member.member_type & (AType::IMethod as i32) != 0 {
                                    members.push(*base_member);
                                }
                            }

                            // process prop members (if its a prop! i love how these expand -- it torques my head to see why it could be)
                            $(
                                members.push(type_member_t {
                                    name: std::ffi::CString::new(stringify!($prop_name)).unwrap().into_raw(),
                                    _type_: std::ptr::null_mut(), // Type info lookup needed
                                    offset: memoffset::offset_of!($name, $prop_name) as i32,
                                    count: 1,
                                    member_type: AMFlag::Prop as i32,
                                    operator_type: 0,
                                    required: 0,
                                    args: meta_t { count: 0, ..Default::default() },
                                    ptr: std::ptr::null_mut(),
                                    method: std::ptr::null_mut(),
                                });
                            )*

                            // process method members
                            $(
                                paste! {
                                    members.push(type_member_t {
                                        name: std::ffi::CString::new(stringify!($method_name)).unwrap().into_raw(),
                                        _type_: std::ptr::null_mut(), // Type info lookup needed
                                        offset: memoffset::offset_of!([<f_ $name>], $method_name) as i32,
                                        count: 1,
                                        member_type: AMFlag::IMethod as i32,
                                        operator_type: 0,
                                        required: 0,
                                        args: meta_t { count: 0, ..Default::default() },
                                        ptr: Self::$method_name as *mut c_void,
                                        method: Self::$method_name as *mut c_void,
                                    });
                                }
                            )*

                            let boxed   = members.into_boxed_slice();
                            let ptr     = Box::into_raw(boxed) as *mut type_member_t;

                            ftable.member_count = members.len() as i32;
                            ptr
                        },
                        traits:     0, // Set based on class traits
                        src:        std::ptr::null_mut(),
                        arb:        std::ptr::null_mut(), // FFI type info
                        meta:       meta_t { count: 0, ..Default::default() },
            
                        // set method pointers
                        $(
                            $method_name: Self::$method_name,
                        )*
                    });
                }
        
                // Register with A-type system
                Box::into_raw(ftable) as *mut f_A
            }
        
            $(
                pub unsafe extern "C" fn $method_name(
                    this: *mut $name $(, $arg: $arg_type)*
                ) -> $ret {
                    unimplemented!()
                }
            )*
        }
    }
}

#[repr(C, packed(1))]
pub struct obj {
    pub _type_:                    *mut f_A,
    pub _scalar_:                  *mut f_A,
    pub refs:                      i64,
    pub data:                      *mut c_void,
    pub shape:                     *mut VecI64,
    pub alloc:                     i64,
    pub count:                     i64
}

A_type!(obj {
    prop pub _type_:                    *mut f_A;
    prop pub _scalar_:                  *mut f_A;
    prop pub refs:                      i64;
    prop pub data:                      *mut c_void;
    prop pub shape:                     *mut VecI64;
    prop pub alloc:                     i64;
    prop pub count:                     i64;

    // Methods from A_schema
    method init()                        -> ();
    method dealloc()                  -> ();
    method compare(other: *mut Au)        -> i32;
    method hash()                        -> u64;
    method copy()                        -> *mut c_void;
    method data()                        -> *mut c_void;
    method data_type()                   -> *mut f_A;
    method data_stride()                 -> i64;
    method with_cereal(s: *const c_char) -> *mut A;
    method cast_string()                 -> *mut String;
    method cast_bool()                   -> bool;
});


fn main() {
    // construct an instance of obj, once it is compilable 
    println!("hi");
}

