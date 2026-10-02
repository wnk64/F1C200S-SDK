# Buildroot Platform Port

The LVGL application talks to the platform only through
`windows_lvgl/src/platform_services.h`. A board implementation must keep the
same signatures and replace the desktop adapter with the following services:

Compile the shared configuration with `-DWILIWILI_BOARD` for the board
profile. This selects RGB565 (`LV_COLOR_DEPTH=16`) and a 192 KiB LVGL memory
pool; the default Windows profile remains ARGB8888 with a 512 KiB pool.

The current application translation unit was syntax-checked with
`-DWILIWILI_BOARD` on 2026-09-08. Linking still belongs to the future
framebuffer/evdev board adapter; the Windows executable is not used as a board
binary.

- Display: choose the physical panel orientation (currently 480x800); use two
  small RGB565 draw strips and DMA flush, not two full-screen draw buffers.
- Input: evdev pointer/buttons; no SDL or window manager.
- Fonts: generated LVGL CJK font; no FreeType runtime.
- HTTPS GET/POST: OpenSSL-backed client with bounded response buffers. GET must
  optionally expose raw response headers for QR `Set-Cookie` handling.
- MD5: OpenSSL EVP or the existing board crypto helper for WBI signing.
- Thumbnails: fixed 186x104 RGB565 decode, release compressed bytes immediately.
- Player: Cedar H.264 Annex-B access units on a separate DE video plane; LVGL
  remains the control/overlay plane.

## Native-Rotation Rule

For the portrait F1C200S display path, playable source streams must be
`800x480` H.264. Cedar rotates the decoded frame by 90 degrees into the native
`480x800` panel plane. This path is **native-size only**: do not request DE,
Cedar, SDL, or LVGL video scaling before or after rotation. LVGL owns a
separate transparent UI plane and may overlay controls, danmaku, or status
over the Cedar plane, but it must never resize the video plane.

The Windows `--board-player` thumbnail is a visual surrogate for that plane.
It is not evidence of a scaled board video path.

Memory limits are intentional: feed/search/history/favorite/later lists are
bounded, danmaku is capped at 12 messages and 2 MiB input, and no desktop
media backend is part of the board contract.

The platform GET contract now applies a 2 MiB default response ceiling before
growing the response buffer. The Buildroot implementation must preserve this
guard and fail oversized responses before allocation, rather than checking
only after a complete download has accumulated in RAM.

POST actions use a separate 512 KiB response ceiling. The board client must
apply that cap while receiving response bytes, including error responses.

## Port Readiness Check (2026-09-30)

The September 27 revision was syntax-checked with `-DWILIWILI_BOARD`;
that result is not a linked ARM binary or acceptance of the latest changes. The Windows
`--board-preview` is 800x480, while the panel described in the September 19
F1C200S handoff is 480x800. Before deployment, verify the current panel and
select an explicit UI rotation/input-coordinate mapping rather than stretching
the Cedar video plane.

The shared `flush()` now calls `platform_display_flush_pixels()` with a
borrowed, tightly packed native LVGL strip, rectangle coordinates and a
`present` flag for the last strip in a refresh. The application no longer
allocates the full ARGB8888 shadow frame. Windows updates only that SDL texture
rectangle; a full ARGB copy exists only in hidden audit instances for snapshots.

The platform must consume the borrowed pixels synchronously before returning:
the current shared callback immediately calls `lv_disp_flush_ready()`. Never
queue a DMA operation using this pointer after return. An asynchronous board
adapter needs owned staging buffers or a redesigned completion callback. RGB565
byte ordering and stride must match the board LVGL configuration; the Windows
16-bit validation profile uses a 512 KiB host pool and FreeType, not the 192 KiB
board profile. Windows' SDL/FreeType/WinHTTP adapter cannot be linked for
Buildroot. Implement the board platform services, including CJK
fonts, HTTPS/header limits and Cedar FIFO playback, in a separate source-backed
candidate before producing a deployable ARM binary.

`platform_decode_thumbnail_lvgl()` optionally reports whether decoded pixels
contain alpha. Opaque sources use `sizeof(lv_color_t)` bytes per pixel and
`LV_IMG_CF_TRUE_COLOR`; transparent sources keep
`LV_IMG_PX_SIZE_ALPHA_BYTE` and `LV_IMG_CF_TRUE_COLOR_ALPHA`. A board decoder
must preserve this contract. Theme colors are now selected in LVGL object
styles, before antialiasing; do not recolor thumbnail or QR pixels in a display
flush callback.

The available F1C200S handoff is dated September 19, 2026; its current
Buildroot source/config/toolchain and board-runtime rollback state have not
been verified for this UI port. Do not replace the active program or rootfs
based on Windows tests. Establish the current reproducible baseline and
rollback path, then validate three physical cold boots, display/touch
orientation, FIFO/Cedar playback, RAM/FPS, and shared-device regressions.
