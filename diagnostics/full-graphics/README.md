# Full graphics diagnostics (opt-in)

This branch adds a bounded, separate graphics diagnostics log. It is observational only: it does not change GPU state, disable features, alter shader translation, add readbacks, or change cache contents.

## Enable

Set these environment variables **before launching Kyty**:

- `KYTY_GRAPHICS_DIAG=1` enables the diagnostics log.
- `KYTY_GRAPHICS_DIAG_FILE=C:\\path\\to\\kyty-graphics-diagnostics.log` optionally selects an absolute output path. If omitted, the file is created in the process working directory as `kyty-graphics-diagnostics.log`.

The log is truncated at the beginning of each new process and capped at 100,000 records. Categories currently emitted by this initial instrumentation are:
- `gpu`: physical-device identity and GPU lifecycle.
- `pipeline`: graphics attachment formats, pixel-stage presence, mesh/tessellation mode and topology.
- `color_target`: guest mip/layer, export mapping and sample/fragment counts.
- `image_view`: guest address, image/view formats, view type, aspect, mip/layer ranges and usage.

The ordinary Kyty log and existing shader/SPIR-V diagnostics remain unchanged. Existing Vulkan validation output remains in the ordinary log if enabled by the build/runtime configuration.

## Scope and limitations

This is the first instrumentation pass, not a claim of exhaustive Vulkan tracing. It does not yet record every draw/dispatch, descriptor write, resource barrier, allocation, render-pass transition, validation callback, or final swapchain/present conversion. No GPU readback is performed because readbacks can change synchronization and timing. The next pass should add these event families only after this pass builds and the first capture is reviewed.

Keep the complete log, including its session header, and note the game scene and approximate capture time when uploading it.
