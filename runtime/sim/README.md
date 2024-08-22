# The HTLB Simulator

This interface allows the testing harness to link against a "simulator" of the HTLB.
It's not part of the core runtime, as we want to link against `libc++` for a few features.