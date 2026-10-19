# ONNX Runtime C API headers

`onnxruntime_c_api.h`, `onnxruntime_error_code.h` and `onnxruntime_ep_c_api.h`
of ONNX Runtime 1.31.0 (Microsoft, MIT licence: `LICENSE`), from the release
package `onnxruntime-linux-x64-1.31.0.tgz`
(https://github.com/microsoft/onnxruntime/releases/tag/v1.31.0).

Only the headers: `src/ki_make.c` (`mtv-dec --make-ki`, the AI upscaling)
loads the library itself at run time (dlopen, LoadLibrary), so the game builds
and runs without it; the launcher loads the library with the model from
https://openmtv.net/ki/1/ (`launcher/ki.go`).
