use std::os::raw::c_char;

#[no_mangle]
pub extern "C" fn rust_bump(n: *mut i32) -> *const c_char {
    unsafe { *n += 1 };
    b"hello from rust\0".as_ptr() as *const c_char
}
