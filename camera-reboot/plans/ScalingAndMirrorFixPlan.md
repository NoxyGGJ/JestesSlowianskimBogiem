# Scaling And Mirror Fix Plan

## Scope
This plan covers the next camera-compatibility pass only. No source changes are included yet.

Target pipeline:
1. capture native camera frame
2. convert to BGRA if needed
3. normalize orientation so the image is top-down
4. scale down to fit within `640x480` while preserving aspect ratio
5. run detection on the scaled frame
6. display debug output from that processed frame

## Implementation Steps

### Step 1: Preserve frame orientation metadata in `CameraPixels`
- Add explicit row-order metadata to `CameraPixels::Frame`.
- Preserve the sign meaning of the negotiated media type height when reading the connected media type.
- Keep public `width` and `height` stored as positive values.
- Set row-order metadata when opening the camera so each frame can be normalized consistently.

### Step 2: Make `readBGRA()` return a normalized top-down image
- Normalize vertical orientation inside `CameraPixels`.
- For bottom-up RGB/BGR/BGRA sources, reverse row order during copy or conversion.
- For top-down sources, keep row order unchanged.
- Keep the normalized output format as BGRA32.
- Do not add any flip logic to detector or display code.

### Step 3: Add an area-based BGRA downscaling stage before detection
- Add a dedicated helper that accepts a BGRA frame and produces a BGRA frame scaled to fit within `640x480`.
- Preserve source aspect ratio.
- Do not upscale sources already smaller than the target bounds.
- Implement downscaling with box-averaging or a stronger area-based method.
- Make the scaler average source pixel coverage when shrinking high-resolution frames.
- Ensure the implementation works correctly for non-integer scale ratios such as `1920x1080 -> 640x360`.
- Keep the helper isolated so the scaling algorithm can be improved later without changing the rest of the pipeline.

### Step 4: Feed the scaled frame into the detector
- Update the main loop so `FeatureDetector::Process(...)` receives the scaled BGRA frame.
- Keep detector internals unchanged unless a concrete issue appears during implementation.
- Ensure `frame.width`, `frame.height`, `frame.stride`, and BGRA layout remain consistent with current detector expectations.

## Target Size Rules
- Maximum processing size: `640x480`
- Preserve aspect ratio at all times.
- Do not upscale smaller inputs.
- Example mappings:
  - `1920x1080 -> 640x360`
  - `1280x720 -> 640x360`
  - `1280x960 -> 640x480`
  - `640x480 -> 640x480`
  - `320x240 -> 320x240`

## Acceptance Criteria
1. The camera image is not vertically flipped.
2. The detector runs on a frame that fits within `640x480`.
3. The source aspect ratio is preserved.
4. The processed frame is the same frame used for debug visualization.
5. High-resolution camera input does not introduce visible aliasing from naive downsampling.
6. Your runtime checks confirm the updated pipeline works with the camera set you care about.
