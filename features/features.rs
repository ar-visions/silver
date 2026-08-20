// rust companion: cbindgen emits the header, rustc builds a staticlib.
// #[no_mangle] + extern "C" is what silver links against
#[no_mangle]
pub extern "C" fn rs_bump(n: *mut i32) {
    unsafe { *n += 1 };
}

#[no_mangle]
pub extern "C" fn rs_mul3(n: i32) -> i32 {
    n * 3
}

#[no_mangle]
pub extern "C" fn rs_sum(v: *const i32, n: i32) -> i32 {
    let xs = unsafe { std::slice::from_raw_parts(v, n as usize) };
    xs.iter().sum()
}
