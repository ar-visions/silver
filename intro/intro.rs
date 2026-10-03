// rust companion: cbindgen writes the header silver imports
#[no_mangle]
pub extern "C" fn triple(n: i32) -> i32 {
    n * 3
}

#[no_mangle]
pub extern "C" fn tally(v: *const i32, n: i32) -> i32 {
    let xs = unsafe { std::slice::from_raw_parts(v, n as usize) };
    xs.iter().sum()
}
