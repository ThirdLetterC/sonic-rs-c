use std::env;
use std::fs;
use std::path::PathBuf;

fn main() {
    println!("cargo:rerun-if-changed=src/lib.rs");
    println!("cargo:rerun-if-changed=include/sonic_rs.h");
    println!("cargo:rerun-if-changed=cbindgen.toml");

    // Copy the embeddable C header alongside the build artifacts in OUT_DIR and target/<profile>/
    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let header_src = manifest_dir.join("include").join("sonic_rs.h");

    if let Ok(out_dir) = env::var("OUT_DIR") {
        let out_path = PathBuf::from(&out_dir);
        let _ = fs::copy(&header_src, out_path.join("sonic_rs.h"));

        // OUT_DIR is typically target/<profile>/build/<pkg-hash>/out
        // Copy sonic_rs.h directly into target/<profile>/ next to libsonic_rs_c.a / sonic_rs_c.lib
        if let Some(profile_dir) = out_path.ancestors().nth(3) {
            let _ = fs::copy(&header_src, profile_dir.join("sonic_rs.h"));
        }
    }
}
