use core::ffi::c_void;

/*
struct Handle {
    handle: u64, // The handle value returned from alaska_alloc
    size: usize, // how big the allocation is
    ptr: *mut c_void,
}
*/

#[no_mangle]
pub extern "C" fn alaska_alloc(size: usize) -> *mut c_void {
    let ptr = unsafe { libc::malloc(size) };
    println!("alloc {ptr:p}");
    ptr
}

#[no_mangle]
pub extern "C" fn alaska_free(ptr: *mut c_void) {
    unsafe { libc::free(ptr) };
}

#[no_mangle]
pub extern "C" fn alaska_pin(handle: *mut c_void) -> *mut c_void {
    println!("pin   {handle:p}");
    return handle;
}

#[no_mangle]
pub extern "C" fn alaska_unpin(handle: *mut c_void) {
    println!("unpin 0x{:016p}", handle);
}

#[no_mangle]
pub extern "C" fn alaska_barrier() {
    // All the pins in this "function" have been unpinned, do something with them
}
