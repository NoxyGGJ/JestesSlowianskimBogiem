# Camera Usability Plan

## Scope
This plan covers the next usability pass only. No source changes are included yet.

Target behavior:
1. the processed debug image is centered in the `640x480` window
2. the available camera list is drawn on screen in the upper-left corner
3. the user can switch cameras with a keypress instead of restarting the program
4. camera availability is refreshed once per second without interrupting the current connection
5. if the active camera disappears, the program switches to another available camera when possible

## Planned Controls
- Camera switch key: `Tab`

## Implementation Steps

### Step 1: Separate debug image placement from overlay UI drawing
- Keep the existing detector output generation unchanged.
- Change the presentation path so the detector image can be drawn at an explicit destination position inside the window.
- Clear the window buffer before each frame so unused areas remain black.
- Draw the detector output centered inside the `640x480` window.
- Keep text overlays independent from the centered image position.

### Step 2: Add a lightweight UI state block in `main.cpp`
- Track the current enumerated camera list in memory.
- Track the currently opened camera index in the latest enumerated list.
- Track the active camera identity used for refresh matching:
  friendly name plus same-name occurrence ordinal from the enumeration that was used when it was opened.
- Track the current camera name shown on screen.
- Track whether multiple cameras are available.
- Track the timestamp of the last device re-enumeration.
- Track the timestamp of the last successfully received frame.
- Track whether the active camera is currently considered disconnected.
- Track a short status line for events such as camera switch, disconnect, reconnect, or open failure.

### Step 3: Draw the camera list on screen in the upper-left corner
- Render the current camera list into the Allegro backbuffer each frame.
- Prefix the active camera with a clear marker.
- Show camera indices exactly as used internally for switching.
- Keep the list anchored to the upper-left corner regardless of image size.
- Keep the list readable when no camera is open.

### Step 4: Add on-screen usage hints
- When more than one camera is available, draw a visible hint for the camera-switch key.
- Draw the current camera name and index on screen.
- Draw the current status line on screen.
- Keep all overlay text outside the centered image logic so layout remains stable.

### Step 5: Implement camera switching by keypress
- Detect a rising-edge press of the camera-switch key so one press advances one camera.
- Only allow switching when at least two cameras are currently enumerated.
- Advance to the next available enumerated camera and wrap around at the end.
- Reopen the camera immediately after a switch request.
- On successful switch, update the active camera state and status line.
- On failed switch, continue trying the next available enumerated camera in wraparound order.
- If no camera can be opened, leave the program running with no active camera and show the failure in the status line.

### Step 6: Re-enumerate devices once per second without disrupting the active camera
- Re-enumerate camera devices from the main loop no more than once per second.
- Do not close or reopen the active camera just because the device list changed.
- Refresh the on-screen device list from the latest enumeration result.
- Update the multi-camera hint visibility from the refreshed list.
- If the active camera's enumerated index changed, update the tracked active index to the matched refreshed index without touching the live connection.

### Step 7: Match the active camera across refreshed enumerations
- Match the currently opened camera against the refreshed device list by friendly name.
- If multiple devices share the same friendly name, match by occurrence order within that name group.
- Keep the tracked active camera index synchronized with the refreshed list when a match is found.
- If the active camera is still present, continue using the current connection unchanged.
- Treat this index synchronization as UI/state maintenance only; it must not force a reopen.

### Step 8: Handle active camera disappearance
- Detect active camera disconnection from read failures.
- Prefer an explicit device or stream failure signal from the underlying Windows APIs if one is available through the wrapper.
- If no explicit disconnection signal is available, treat `3` seconds without receiving a frame as a disconnection.
- When a disconnection is detected, check the most recent enumerated list before deciding recovery.
- If the active camera is still present in the latest enumeration but the stream appears dead, try reopening that same matched camera first.
- If reopening the matched camera fails or the active camera is no longer present, try to open another available camera automatically.
- Prefer the next available camera after the previous one; wrap around if needed.
- If no cameras are available, close the current camera state cleanly and continue running with a visible status message.
- Continue periodic re-enumeration so a later camera reconnection can be detected.

### Step 9: Handle camera return after temporary loss
- If no camera is currently open and the periodic enumeration discovers devices again, automatically try to open one.
- Prefer the previously active camera when it is present again, matched by friendly name plus same-name occurrence ordinal.
- Otherwise open the first available camera and update the status line.

## Matching Rules
- Camera identity for refresh logic: friendly name plus same-name occurrence ordinal in the enumerated list.
- Active camera switch order: follow the current enumerated list order.
- Automatic fallback after disconnect: try the next available camera in enumerated order.
- Re-enumeration may update the tracked active index, but must not interrupt a still-working live camera connection.

## Disconnect Rules
- Use any explicit camera or stream failure surfaced by the Windows capture stack as an immediate disconnect signal when available.
- Otherwise, treat `3` consecutive seconds without a successfully received frame as a disconnect.
- Short read timeouts during normal operation are not by themselves a disconnect if frames resume before the `3`-second threshold.

## Acceptance Criteria
1. The processed detector image is centered in the `640x480` window.
2. The camera list is visible in the upper-left corner.
3. The active camera is clearly marked on screen.
4. When more than one camera is available, the switch-key hint is visible on screen.
5. Pressing the switch key changes to the next available camera and wraps correctly.
6. Camera enumeration refreshes once per second without interrupting a still-working active camera.
7. If the active camera is disconnected, the program switches to another available camera when possible.
8. If no cameras are available, the program stays running and reports the situation on screen.
9. If cameras return later, the program reconnects automatically in a reasonable way.

