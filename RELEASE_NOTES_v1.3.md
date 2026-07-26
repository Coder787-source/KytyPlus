# KytyPlus v1.3

**Download:** `KytyPlus-v1.3-windows-x64.zip`

What's new vs v1.2:

Audio

- NGS2: fixed a use-after-free where destroying a rack or system left it linked in the global render list, so a later render pass on another system could walk into freed memory (crash risk on titles that create/destroy audio racks during gameplay, e.g. level loads)
- AJM: Opus decode is now backed by a real decoder (ffmpeg) instead of a stub that silently returned zeroed audio — titles using AJM Opus should now get actual sound instead of permanent silence

Video

- `libVideoDec2`: decoder handle lookup now holds its lock for the full duration of Decode/Flush/Reset, closing a use-after-free race against a concurrent decoder delete
- `AvPlayer`: fixed a heap buffer overflow in video frame preparation when a decoded frame's dimensions differ from the cached stream dimensions (e.g. mid-stream resolution change); crop offset math adjusted to match
- `AvPlayer`: fixed missing locks on Pause/Resume/CurrentTime/Enable/Disable/Change that could race with playback decode
- `AvPlayer`: fixed premature end-of-stream handling that could drop already-buffered packets of one stream (audio or video) when the other hit EOF first, truncating playback early

Input

- Pad: touch point IDs are no longer reported as active when the touch itself isn't active
- Pad: gyro/accel/touch data returned from a multi-sample `PadRead` call is no longer duplicated identically across historical buffered samples — only the most recent sample carries the polled motion state, avoiding fabricated motion data for titles doing per-sample gyro integration
- Pad: fixed a missing lock on active-controller-id and motion-enabled state reads

Expectations

- These are correctness/stability fixes in the audio, video, and input HLE paths — not new feature support or a playability claim
- Further boot/gameplay reach on affected titles is still not the same as "playable"; verify with your own logs before reporting a title as working

Tag `v1.3`, GPL-2.0.
