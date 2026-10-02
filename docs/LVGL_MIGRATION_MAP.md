# Windows LVGL Migration Map

This worktree replaces rendering only. The upstream `yoga` branch at
`88e5876bea9502d06f46a8656e3530684d3aaf7d` remains the behavior contract.

## Board Target

The F1C200S board target is fixed at **800x480**. The present Windows proof
uses a 1280x720 desktop canvas and must not be copied to the board unchanged.
Board migration requires a separate compact layout profile: 64px navigation
rail, two-column card grids, framebuffer/DE LVGL display backend, and a Cedar
video plane kept separate from the LVGL UI plane. `libmpv` is a Windows desktop
backend option only and is not a board dependency.

The Windows executable supports `--board-preview` for this profile. It uses an
actual 800x480 SDL/LVGL framebuffer; its 2026-09-06 capture is
`windows_lvgl_board_800x480_verified.png`.
The Home-to-detail compact route is independently captured as
`windows_lvgl_board_detail_800x480_verified.png` through `--board-detail`.
The compact Player route is independently captured as
`windows_lvgl_board_player_800x480_verified.png` through `--board-player`.
It verifies that the selected live-feed model reaches the 800x480 player
surface. It is deliberately a UI-only verification: no desktop media engine
is used and no video has been presented by Cedar in this Windows capture.
The main.xml navigation shell now has a true 64px compact profile: icon-like
Home/Dynamic/Mine rail entries plus Inbox and Settings routes. Dynamic, Mine,
Inbox, and Settings each render inside the 720x382 board content area rather
than falling through to desktop-size panels. Their captures are
`windows_lvgl_board_dynamic_800x480_verified.png`,
`windows_lvgl_board_mine_800x480_verified.png`,
`windows_lvgl_board_inbox_800x480_verified.png`, and
`windows_lvgl_board_settings_800x480_verified.png`.

### Board dependency boundary

The current Windows proof uses SDL2, SDL2_image, FreeType, and WinHTTP only as
desktop adapters: respectively window/input/present, thumbnail decoding,
desktop font discovery, and HTTP. They are not the F1C200S runtime contract.
The board port must replace them with a fixed 800x480 DE/framebuffer LVGL
backend, evdev input, pre-generated LVGL Chinese fonts, and a small existing
Buildroot HTTP/TLS client. The video backend must remain the existing Cedar
decoder and a separate DE video plane. `libmpv` is neither linked by this
rewrite nor permitted as a board dependency.

The initial replacement seam is now explicit in
`windows_lvgl/src/platform_services.h`. Windows implements it in
`platform_windows.cpp`; `main.cpp` calls only the interface for configuration,
external links, and HTTPS payloads. A future `platform_buildroot.cpp` must
implement those same services without SDL2, WinHTTP, or ShellExecute.

The same interface now owns desktop window creation, pointer/keyboard input,
tick timing, ARGB8888 present, and 186x104 thumbnail decoding. `main.cpp`
contains no SDL, SDL_image, WinHTTP, Shell, or Windows headers. This was
rebuilt and re-captured through `--board-player` on 2026-09-06; it is the
required source boundary before a DE/framebuffer implementation is added.

### Buildroot audit, 2026-09-06

Read-only inspection of `/home/wnk/LicheePi_Nano/buildroot-2018.02.11/.config`
and `output/target` found OpenSSL, FreeType, and JPEG already present. LVGL,
SDL2, libdrm, curl, JSON libraries, and WebP are not enabled. Therefore the
board candidate must vendor LVGL and use a small OpenSSL-backed HTTPS service;
it must not add SDL2, libmpv, curl, or a runtime desktop font engine. FreeType
is now also platform-local: `main.cpp` requests `platform_ui_font()` and the
Windows adapter supplies the desktop font. The Buildroot adapter will supply
a generated LVGL Chinese font instead.

## Implemented vertical slice

| Upstream contract | LVGL status | Validation requirement |
| --- | --- | --- |
| `activity/main.xml` shell navigation | Partial: Home and Dynamic are routed LVGL screens; Mine/Inbox/Settings remain route placeholders | Windows capture |
| `fragment/home_tab.xml` top tabs | Partial: visual tabs only | Windows capture |
| `fragment/home_hots_all.cpp` public Popular data | Partial: public Popular feed uses the shared LVGL grid/detail model | Windows capture: active Popular tab and live Popular cards |
| `activity/video_activity.xml` detail route | Partial: selected title/UP/view/BV, real cover and internal back route; player is still pending | Verified on Windows: click a live Recommend card and compare title/UP/view/BV in the in-app detail screen |
| `fragment/home_recommends.cpp` recommend data | Partial: real feed, decoded covers, cards and selected-video route. Current endpoint accepts the unsigned compatibility request; upstream WBI signing still must be ported. | Windows capture plus selected BV comparison |
| `fragment/dynamic_tab.xml`, `dynamic_tab.cpp` | Visual structure migrated: UP selector column plus independent Video/Article panes. The original presenter refuses dynamic data without login, so the LVGL route truthfully shows its signed-in requirement; cookie/login and dynamic API parsing are pending. | 2026-09-06 Windows capture: `windows_lvgl_dynamic_verified.png` |
| `fragment/mine_tab.xml`, `mine_tab.cpp` | Visual structure migrated: user card, four statistics, and History/Favorites/Subscription/Anime/Series/Watch later tabs. It starts in the upstream `onUserNotLogin()` state until QR login, cookies, and data presenters are ported. | 2026-09-06 Windows capture: `windows_lvgl_mine_verified.png` |
| `fragment/inbox_view.xml`, `main_activity.cpp` | Visual structure migrated: source-sized right-side drawer and Chat/Replies/@ mentions/Likes tabs. The route retains `DialogHelper::checkLogin()` semantics; message APIs and send flow remain pending. | 2026-09-06 Windows capture: `windows_lvgl_inbox_verified.png` |
| `activity/setting_activity.xml` | Partial: Playback/Interface/Other navigation and a subset of LVGL switch controls are migrated. `ProgramConfig` JSON persistence and the remaining selectors are pending. | 2026-09-06 Windows capture: `windows_lvgl_settings_verified.png` |
| `fragment/search_history.xml`, `search_history.cpp` | Behavior migrated: persistent, de-duplicated, capped-at-50 search history, reverse display, query selection, and clear operation. The LVGL-owned history file is separate while full `ProgramConfig` compatibility is pending. | 2026-09-06 Windows capture: `windows_lvgl_history_verified.png` |
| `search_activity.xml`, `search_tab.cpp`, `search_video.cpp` | Search route migrated as a distinct LVGL screen: query heading, source-ordered Video/Bangumi/Cinema/Hots/History tabs, real public Video search results, thumbnail cards, and detail return preserving the result snapshot. Other categories remain explicit visual tabs until their source APIs are ported. | 2026-09-07 Windows capture: `windows_lvgl_board_search_800x480_verified.png`; launch with `--board-search LVGL` |
| `activity/player_activity.xml`, `player_activity.cpp` | Partial: detail Play now routes into an LVGL Player surface with selected live-feed metadata, media region, source-derived Comments/Parts/Related panels, and LVGL Play/Pause state control. MPVCore/Cedar bridge, stream URL resolution, danmaku, comments data, and authenticated actions remain pending. | 2026-09-06 Windows capture: `windows_lvgl_player_verified.png` |

## Activity migration inventory

| Activity XML | Destination LVGL screen | Required behavior |
| --- | --- | --- |
| `main.xml` | Application shell | Home/Dynamic/Mine/Inbox/Settings navigation |
| `search_activity*.xml` | Search screen | history, suggestions, result tabs |
| `video_activity.xml` | Video detail | metadata, parts, related, actions, player route |
| `player_activity.xml`, `live_player_activity.xml` | Player screens | player controls, danmaku, quality, settings |
| `gallery_activity.xml`, `hint_activity.xml` | Modal/gallery | image and hint navigation |
| `pgc_index_activity.xml` | PGC index | filters and season detail |
| `setting_activity.xml` | Settings | persisted upstream settings contract |

## Fragment migration inventory

| Family | Upstream fragments | Dependencies |
| --- | --- | --- |
| Home | `home_live`, `home_recommends`, `home_hots*`, `home_bangumi`, `home_cinema` | Home presenters, WBI/API, card views, recycler |
| Search | `search_tab`, `search_video`, `search_bangumi`, `search_cinema`, `search_hots`, `search_history`, `search_order` | search API, history config, intent |
| Mine | `mine_tab`, `mine_history`, `mine_later`, `mine_collection`, `mine_bangumi*`, `mine_qr_login` | cookies, QR login, user APIs |
| Dynamic/Inbox | `dynamic_*`, `inbox_*` | authenticated feed/message APIs |
| Player | `player_*`, `season_evaluate`, `share_*` | MPV abstraction, comments, collection, danmaku |
| Settings | `settings_*`, `setting_network` | ProgramConfig persistence |
| Space | `space_tab` | user-space API and video cards |

## Non-negotiable cross-cutting work

1. Port `Intent` routes before expanding page count; no LVGL screen may invent navigation semantics.
2. Port HTTP, WBI signing, cookies, QR login, and JSON result types from `wiliwili/include/api/`.
3. Port reusable card/recycler, image cache, i18n, focus/keyboard/gamepad input, and settings persistence.
4. Abstract player rendering and controls so Windows can keep the upstream player while the board backend is substituted later.
5. Mark every screen as `not started`, `visual only`, `data integrated`, or `behavior verified`; no blanket completion claim.

## Windows verification

- 2026-09-06: `windows_lvgl` was rebuilt from the LVGL rewrite tree through an
  ASCII-only build path (`C:\\lvgl_wiliwili_build`) because MinGW/Ninja cannot
  reliably emit object files under the non-ASCII source path. The source stays
  in `E:\\LVGL界面\\wiliwili_lvgl\\lvgl_rewrite`.
- The target now copies its MinGW runtime DLL closure after build, so direct
  launch no longer depends on an MSYS2 terminal's `PATH`.
- Recommend card selection was verified against the live Bilibili feed. Event
  user data is an index into an immutable displayed snapshot, rather than a
  pointer into `pending_videos`; refreshes can therefore not make a detail
  screen resolve to a stale or previous card.
- 2026-09-06: Popular startup was verified using the same card implementation.
  The visible active tab state now follows the actual data source rather than
  always highlighting Recommend.
- 2026-09-06: `wiliwili_lvgl --dynamic` was captured through `PrintWindow`.
  It verifies the source-derived Dynamic two-column structure and that the
  shared header hides Home-only sub-tabs outside Home. It does not verify
  authenticated dynamic data, because upstream `DynamicVideoRequest` returns
  `no_login` until cookie/login migration exists.
- 2026-09-06: Mine, Inbox, and Settings startup routes were captured through
  `PrintWindow`. The LVGL FreeType font lacks LVGL private-use icon glyphs;
  Inbox therefore uses a literal `X` close label instead of `LV_SYMBOL_CLOSE`,
  which otherwise stalls software glyph rendering on Windows.
- 2026-09-06: LVGL now owns a compatible persistent subset at
  `%LOCALAPPDATA%\\xfangfang\\wiliwili_lvgl\\wiliwili_config.json`. It stores
  the upstream key names `PLAYER_HWDEC`, `PLAYER_AUTO_PLAY`, and `TLS_VERIFY`.
  The file was verified after a Windows launch; full `ProgramConfig` migration
  remains required for all other settings, cookie state, and search history.
- 2026-09-06: Search history now follows `SearchHistory` and
  `ProgramConfig::addSearchHistory`: a query is de-duplicated, appended to a
  50-item chronological store, then displayed newest-first. It was verified by
  launching `--search LVGL`, restarting with `--history`, and capturing the
  persisted entry.
- 2026-09-06: `--player` obtains the live Recommend feed then routes its first
  actual card through the same Player path used by the detail Play button. The
  capture proves source data propagation, not media playback: no MPVCore or
  stream URL backend has been ported yet.
- 2026-09-07: Search now has a separate LVGL route instead of reusing the Home
  grid. `--board-search LVGL` fetched real Bilibili Video results, decoded
  thumbnails, and displayed the source-ordered SearchTab categories. The host
  desktop applies 125% display scaling to the SDL window capture; the board
  profile remains authored at 800x480 physical pixels.
- 2026-09-07: Player controls now have a real LVGL Play/Pause state transition
  on both desktop and compact board layouts. The callback is intentionally
  backend-neutral; Cedar/stream control can be attached through the platform
  player bridge without changing the LVGL screen tree.
- 2026-09-08: Thumbnail ownership is now explicit: after the platform decoder
  produces the fixed 186x104 LVGL image, the compressed network buffer is
  released from the feed model. The board adapter therefore needs only a small
  JPEG/PNG-to-RGB(A)565 decoder and never has to retain both compressed and
  rendered copies for every card.
- 2026-09-08: Feed replacement now deletes the prior LVGL page before its
  thumbnail backing buffers are released. This keeps LVGL image descriptors
  from observing dangling data during a live refresh or search transition.
- 2026-09-08: The public Home and Search requests now follow upstream WBI
  signing: `/nav` keys are cached for one hour, the upstream mixin table is
  applied, and the signed query uses a platform MD5 service. Windows verifies
  this with BCrypt; the Buildroot adapter can supply the same MD5 operation
  through its existing OpenSSL dependency. `--popular` was captured with real
  Bilibili cards in `windows_lvgl_wbi_popular_verified.png`.
- 2026-09-08: The LVGL Player surface now includes source-equivalent transport
  primitives for start/pause, position seek in milliseconds, and 0-100 volume.
  These are platform services rather than MPV calls; the compact profile keeps
  them inside the 720x382 UI plane. `windows_lvgl_board_player_controls_verified.png`
  verifies the unbound-backend state is surfaced honestly in Windows.
- 2026-09-08: The platform player contract is now deliberately constrained to
  H.264 Annex-B access-unit input and a separate video plane. Windows accepts
  that contract but has no decoder bound; the Buildroot implementation will
  map it to the existing Cedar input and DE layer without inheriting MPV or
  SDL video dependencies.
- 2026-09-08: Windows now has an explicit opt-in Annex-B verification backend:
  `--h264-test <raw-h264-file-or-uri>` launches the system `ffplay` test
  decoder with `-f h264`, while the LVGL control plane remains independent.
  A generated 320x180 Annex-B stream was probed as H.264 and its active ffplay
  output was visually captured in `windows_lvgl_h264_test_state.png`. This is
  test-only and is not a Buildroot dependency or a replacement for Cedar.
- 2026-09-08: `VideoActivity` now loads the real WBI `view` payload before
  rendering detail content, carrying CID, duration, and description into the
  immutable selected video snapshot. `--detail <BV>` provides a direct BV
  route equivalent to the upstream Intent path; `--detail BV1qCty6fEPn` was
  verified in `windows_lvgl_direct_detail_api_verified.png`.
- 2026-09-08: Detail payload `pages` now maps to a bounded CID/part/duration
  list. The first page is selected by default and LVGL exposes a compact Part
  cycle control for multi-P videos, updating the CID that the H.264 player
  contract will later resolve.
- 2026-09-08: Detail resolution now also calls the WBI `playurl` endpoint at
  board-friendly `qn=16`, retaining the real Bilibili media URL separately
  from the raw Annex-B player input. `--player-bv BV1qCty6fEPn` verified the
  `view -> CID -> playurl -> Player` chain in
  `windows_lvgl_playurl_resolved_verified.png`; container demux to H.264
  access units remains deliberately separate.
- 2026-09-08: Media-source parsing now accepts only the first Bilibili
  `durl` URL or the first DASH `baseUrl/base_url`, recording `durl` versus
  `dash` explicitly, with DASH `backupUrl/backup_url` fallback. This keeps the future board demux contract deterministic
  and avoids treating unrelated JSON URL fields as playable media.
- 2026-09-08: Video detail now retains aid and comment count, then requests
  only four public top-level comments through the WBI reply API. The Player
  Comments panel renders those compact author/message records without a
  scrolling cache; `windows_lvgl_comments_api_verified.png` verifies real
  response data.
- 2026-09-08: The Player Parts panel now renders real detail `pages` and
  re-resolves the selected part's CID and media URL before returning to the
  player. `--player-bv-parts BV1qCty6fEPn` verified the bound panel in
  `windows_lvgl_parts_api_verified.png`.
- 2026-09-08: Related videos now load asynchronously from the public archive
  endpoint and populate a bounded four-item LVGL side panel; the parser was
  corrected to walk top-level `data[]` objects. Search Hots, Bangumi, and
  Cinema now have independent lightweight result models and routes. Hots and
  Bangumi were visually verified; the Cinema endpoint returned a non-JSON
  anti-bot response for the test keyword and is kept as an honest empty state.
- 2026-09-08: A lightweight Cookie-session foundation now persists the raw
  Bilibili Cookie separately from UI settings, attaches it to shared HTTPS
  requests, and probes `/nav` for the signed-in display name. Mine updates
  from its login-gated state once a session is accepted. Import is available
  through `--cookie-file <single-line-cookie-file>`; QR generation and the
  authenticated Dynamic/Inbox/Mine data presenters remain separate work.
- 2026-09-08: The platform seam now includes HTTPS form POST. Windows uses
  WinHTTP with an explicit HTTPS-only, URL-encoded request path; the future
  Buildroot adapter can map the same operation to its existing OpenSSL client.
  A minimal `/x/v2/history/report` integration now runs only after the player
  backend reports a successful start. It uses `DedeUserID` and `bili_jct` from
  the imported Cookie, the selected video's aid/cid/duration, `type=3`, and
  `dt=9`. Opening a detail page or merely resolving a play URL does not report
  history. Periodic progress updates and authenticated like/favorite actions
  remain pending until a real player clock and account presenters are ported.
- 2026-09-08: A bounded danmaku slice is now wired to the player path. After a
  successful player start, the app requests `https://comment.bilibili.com/<cid>.xml`,
  rejects responses over 2 MiB, decodes a maximum of 12 short `<d>` messages,
  and publishes them to the LVGL player overlay on the UI thread. The parser
  handles the common XML entities without adding a dependency. The board
  adapter can replace this fetch with a streaming/capped implementation and
  route the resulting text to a DE overlay; timed positioning and full
  scrolling danmaku remain pending.
- 2026-09-08: Dynamic now has an authenticated data path. When a valid Cookie
  session is present, the LVGL route requests the upstream desktop Dynamic
  feed and parses a bounded list of title/summary rows on a worker thread;
  results are published on the LVGL thread. Without a signed-in session it
  keeps the explicit login-required state. The current parser intentionally
  avoids a JSON dependency and remains a compact first page; article-specific
  cards, UP selector data, pagination, and actions remain pending.
- 2026-09-08: Inbox now uses the same authenticated transport for the Reply,
  @mention, and Like feed tabs. Each request is worker-threaded and publishes
  at most eight sender/message rows to the LVGL thread; the Chat tab currently
  shares the bounded reply fallback because the separate `api.vc.bilibili.com`
  session service needs its own host-aware transport. Unauthenticated users
  still receive the explicit login state, and no message is fabricated.
- 2026-09-08: The Inbox transport is now host-aware for the Chat tab as well:
  the session endpoint is requested from `api.vc.bilibili.com` through the
  existing HTTPS service instead of being incorrectly forced onto
  `api.bilibili.com`. Its response is still parsed conservatively into the
  bounded sender/message model; session-specific fields and message-thread
  fetching remain pending.
- 2026-09-08: Mine/History now requests the upstream history cursor endpoint
  when a Cookie session is active. A bounded six-item `Video` model is shared
  by desktop and the 800x480 profile; the compact profile renders three rows
  without retaining extra image buffers. Favorites, subscriptions, anime,
  series, and watch-later presenters remain pending.
- 2026-09-08: The authenticated Mine header now also probes `/x/space/myinfo`
  after `/nav` succeeds and displays the returned dynamic/following/follower/
  coin counts. The probe reuses the existing worker and Cookie transport; no
  additional runtime dependency or persistent payload was added.
- 2026-09-08: Mine now requests the official web QR token and renders its URL
  with LVGL's built-in QR widget in both desktop and compact profiles. QR
  polling and Set-Cookie import are intentionally still pending until the
  platform HTTP contract exposes response headers.
- 2026-09-08: The Mine login entry now requests the official web QR token and
  renders the returned login URL with LVGL's built-in QR widget. This keeps QR
  generation in the existing LVGL dependency and works in both desktop and
  compact profiles. Polling the QR state and importing Set-Cookie from the
  passport response still require response-header support in the platform
  service; until then the QR screen is generation-only and does not claim a
  completed login.
- 2026-09-08: Windows HTTPS GET now optionally captures raw response headers
  without changing existing callers. QR login polling uses this path to query
  the passport status endpoint and merge returned `Set-Cookie` values into the
  persisted Cookie session after a successful scan. Polling is bounded to 30
  attempts at three-second intervals; failure leaves the current session
  untouched. The board adapter must provide the same header capture contract
  using its OpenSSL HTTP implementation.
- 2026-09-08: QR polling no longer skips when an older Cookie is already
  present. It now tracks an independent completion state, trims whitespace
  before merging each `Set-Cookie`, and replaces the persisted session only
  after the passport endpoint reports success.
- 2026-09-08: Mine Favorites now performs the two-step authenticated request
  for the user's created folders and the first six resources in the default
  folder. Results reuse the existing lightweight `Video` model and render in
  desktop and 800x480 profiles; no image cache is retained for this list.
  Favorite actions, folder selection, and the remaining Mine tabs remain
  pending.
- 2026-09-08: Mine Watch Later now requests `/x/v2/history/toview/web` in the
  authenticated session and displays a bounded list in desktop and 800x480
  profiles. It reuses the same lightweight parser and does not retain image
  buffers; item removal and the remaining Mine tabs remain pending.
- 2026-09-08: Dynamic video and article tabs now use their distinct upstream
  endpoints (`feed/video` versus `feed/all`). Switching tabs clears the old
  bounded model before requesting the new source, preventing stale article or
  video rows from being shown under the wrong tab.
- 2026-09-08: Added `--qr-login` as a direct verification route. It opens the
  Mine QR screen and starts token retrieval without requiring pointer input;
  this is useful for board smoke tests where UI automation is unavailable.
- 2026-09-08: The shared `lv_conf.h` now explicitly disables LVGL's PNG, BMP,
  SJPG, GIF, FFmpeg, and Rlottie extensions. The board path decodes thumbnails
  in its platform adapter and sends video to Cedar, so these extensions would
  only consume firmware space. QR and desktop FreeType remain enabled for the
  corresponding host/desktop profiles.
- 2026-09-08: `lv_conf.h` now has an explicit `WILIWILI_BOARD` profile:
  board builds select RGB565 and a 192 KiB LVGL pool, while Windows keeps its
  ARGB8888/512 KiB settings. The profile switch is compile-time only, so no
  runtime memory or dependency cost is added to the desktop proof.
- 2026-09-08: The desktop Dynamic route now loads the authenticated UP list
  from the upstream uplist endpoint and renders at most six names in its
  existing Following rail. It is intentionally omitted from the compact
  profile, where the rail is not present.
- 2026-09-08: Mine now exposes a local Sign out action in both profiles. It
  clears the persisted Cookie and all authenticated in-memory models before
  returning to the logged-out Mine view; it does not require a remote logout
  request or add a dependency.
- 2026-09-08: Mine Subscription now requests the authenticated collected-folder
  endpoint and renders a bounded six-title list in both profiles. This covers
  the source's UGC collection subscription route; opening individual seasons
  and other subscription types remain pending.
- 2026-09-08: Favorites and Watch Later now use a loose video-list parser that
  does not require the search endpoint's `pubdate` field. This matches the
  different response shapes returned by the authenticated Mine APIs and
  prevents valid entries from being silently discarded.
- 2026-09-08: Mine Anime now requests the authenticated Bangumi follow-list
  endpoint and renders a bounded six-title list in desktop and compact
  profiles. The series/TV follow type and season detail routes remain pending.
- 2026-09-08: Mine Series now requests the same Bangumi follow-list API with
  its drama/series type. It has an independent bounded cache so Anime and
  Series data cannot be displayed under the wrong tab.
- 2026-09-08: Danmaku now retains bounded XML `p` timestamps alongside text.
  The player overlay filters to a three-second playback window instead of
  showing every message at once. Windows uses its test-start monotonic time;
  the board adapter will replace that source with Cedar's decode clock.
- 2026-09-08: Video detail now exposes an explicit Like action. It issues the
  upstream archive-like form POST only after user input, using the active
  session's CSRF token; unauthenticated sessions receive a local prompt and
  perform no request.
- 2026-09-08: Playback history reporting now sends the user-selected seek
  position after a successful seek operation, in addition to the initial
  start report. It does not invent periodic progress from the Windows preview
  clock; continuous board reporting remains dependent on Cedar clock support.
- 2026-09-08: Desktop video detail now includes a bounded one-line comment
  field and explicit submit action. It sends the upstream reply-add form POST
  only after user input with a valid CSRF Cookie. The compact layout leaves
  comment composition out to preserve its video-first controls.
- 2026-09-08: Text input focus is now shared safely between Search and the
  desktop comment field. Platform text events route to the focused textarea;
  Enter submits a comment when that field owns focus and otherwise retains the
  existing Search behavior.
- 2026-09-08: Like feedback now reports only that the request was sent; it no
  longer claims success before a response parser confirms it.
- 2026-09-08: Detail Like and Comment actions now parse the API response code
  on their worker threads and publish accepted/failed feedback on the LVGL
  thread. This preserves the no-blocking UI model and prevents false success
  confirmation when the service rejects an action.
- 2026-09-09: Detail Favorite now resolves the account's default created
  folder only after explicit user input, then submits the resource-deal POST.
  It uses the same bounded worker/result path as Like and Comment; it does not
  prefetch or retain folder lists in the player/detail memory footprint.
- 2026-09-09: Inbox Chat sessions can now open a bounded eight-message thread
  through the upstream session-message endpoint. A single active thread is
  kept in memory and a Back action returns to sessions; Reply/@/Like remain
  their source-specific summary feeds.
- 2026-09-09: The active Inbox Chat thread now has a bounded one-line message
  composer and Send action. It posts only after explicit input with a valid
  session/CSRF token, uses the same response-code feedback path, and reuses
  the shared text-focus routing without adding a new UI dependency.
- 2026-09-09: Accepted Chat sends now trigger a bounded thread refresh, and
  Inbox page reconstruction clears stale message-input focus before deleting
  the old LVGL tree.
- 2026-09-09: The 800x480 Inbox route now loads the same authenticated
  Chat/Reply/@/Like summary model as desktop and renders at most four compact
  rows. It retains the compact profile's low-memory limits instead of falling
  back to a login-only placeholder after a valid session is available.
- 2026-09-09: Compact Inbox Chat rows can now open the same bounded active
  thread as desktop and return to the session list. Compact chat composition
  remains deliberately omitted to preserve the 800x480 interaction budget.
- 2026-09-09: Accepted detail comments now trigger the existing detail refresh
  path, so the bounded public-comment panel is reloaded after a successful
  submission rather than remaining at its pre-submit snapshot.
- 2026-09-09: Authenticated Dynamic rows now expose a bounded Like action using
  the upstream dynamic-like endpoint and current `bili_jct`. The action is
  available in desktop and compact profiles only when a dynamic ID is present;
  it never sends a request for anonymous or malformed rows.
- 2026-09-09: Accepted detail favorites now invalidate and refresh the bounded
  Mine Favorites model in the background, so later navigation reads the
  updated server-side list.
- 2026-09-09: Windows screenshot audit found the LVGL switch renderer was
  invisible and the replacement toggle was initially positioned outside the
  row under DPI scaling. Settings controls now use explicit ON/OFF buttons
  aligned to the row edge; desktop and 800x480 layouts share the same
  persistent callback and remain within their containers.
- 2026-09-09: Player Related rows are now navigable. Selecting one stops the
  current platform player, replaces the bounded current detail model with the
  selected BV, and reuses the existing detail fetch path.
- 2026-09-09: Added keyboard/gamepad-style D-pad routing. Windows maps the
  arrow keys into the shared platform event contract; when text input is not
  focused, Left/Right cycles Home, Dynamic, Mine, Inbox, and Settings while
  Up/Down provide a quick return/entry path. This keeps board evdev adapters
  free of SDL-specific key codes.
- 2026-09-09: Added a persistent LVGL UI scale setting with 75/100/125/150%
  choices in desktop and compact Settings. Desktop applies LVGL's native
  transform zoom to the screen; the board profile deliberately keeps physical
  800x480 geometry at 100% and records the selected value for host preview.
- 2026-09-09: Added the source-defined playback settings
  `PLAYER_AUTO_FULLSCREEN` and `PLAYER_LOW_QUALITY` to the LVGL configuration
  and both desktop/compact Settings pages. They persist beside hardware
  decoding and autoplay; the platform player still decides how those flags map
  to a concrete backend.
- 2026-09-09: Added persistent interface settings matching the original
  ProgramConfig names: `SHOW_FPS`, `HIDE_BOTTOM_BAR`, and
  `PLAYER_OSD_TV_MODE`. They are rendered as bounded ON/OFF controls in both
  desktop and 800x480 Settings; backend-side visual effects remain separate
  platform work.
- 2026-09-09: Bound two interface settings to live LVGL behavior:
  `HIDE_BOTTOM_BAR` now hides/restores the navigation rail immediately, and
  `SHOW_FPS` creates a one-second rolling FPS label on desktop and compact
  screens. The values remain persisted under their source-compatible keys.
- 2026-09-09: `PLAYER_OSD_TV_MODE` now changes the Player surface status text
  to the compact TV OSD mode indicator on both desktop and 800x480 layouts.
  The actual video-plane control policy remains platform-player work.
- 2026-09-09: Added desktop playback selectors for source-compatible
  `VIDEO_QUALITY` (qn), `AUDIO_QUALITY`, and `PLAYER_INMEMORY_CACHE`. Each is a
  bounded cycling control with JSON persistence; the compact board profile
  continues to expose only controls that fit its fixed 800x480 budget.
- 2026-09-09: The compact 800x480 Settings playback page now exposes a single
  bounded Quality profile control that cycles coordinated qn/audio/cache
  presets and persists all three values, keeping the board layout usable.
- 2026-09-09: Desktop Playback Settings now allow vertical scrolling so the
  added quality, audio, and cache controls remain reachable instead of being
  clipped below the fixed 720px window.
- 2026-09-09: Fixed `HIDE_BOTTOM_BAR` initialization so a persisted true value
  hides the navigation rail during shell construction after restart, matching
  the immediate toggle behavior.
- 2026-09-09: Screenshot regression now confirms desktop Playback controls
  visibly show `[ON]`/`[OFF]` in their row titles; the previous invisible-child
  rendering issue is closed for both the desktop and compact implementations.
- 2026-09-09: Player volume now follows the source-compatible `PLAYER_VOLUME`
  setting. The value is clamped to 0-100, restored on both desktop and compact
  Player pages, and saved whenever the slider changes.
- 2026-09-09: Added source-compatible `PLAYER_DEFAULT_SPEED` persistence and
  a Player speed-cycle control (0.5x through 2.0x) on desktop and compact
  layouts. The selected speed is carried in `PlatformPlayerSpec` for future
  decoder backends.
- 2026-09-09: Windows platform input now initializes SDL GameController and
  maps controller D-pad/A/B events to the shared Left/Right/Up/Down/Enter/
  Escape contract. This keeps keyboard and gamepad navigation on the same
  LVGL routing path; board evdev adapters can emit the same enum values.
- 2026-09-09: Added the F11 fullscreen shortcut to the shared Windows input
  path. Escape now exits fullscreen before leaving detail/application, matching
  the expected desktop player interaction while leaving board preview fixed.
- 2026-09-09: `PLAYER_VOLUME` now flows through `PlatformPlayerSpec` into the
  opt-in Windows Annex-B test backend, which launches ffplay with the selected
  `-volume` value. This keeps the persisted Player slider and test output in
  agreement without adding a board dependency.
- 2026-09-09: `PLAYER_DEFAULT_SPEED` now also reaches the Windows Annex-B test
  backend. The ffplay verification command applies a `setpts` multiplier while
  the board adapter receives the raw percentage in `PlatformPlayerSpec`.
- 2026-09-09: FPS persistence now accepts the original `HIDE_FPS` key as well
  as the LVGL-facing `SHOW_FPS` key, and writes both forms for compatibility
  with existing ProgramConfig files.
- 2026-09-09: Moved the compact Player speed control from the bottom control
  strip to the video surface corner, avoiding overlap with seek time and volume
  controls at the fixed 800x480 layout.
- 2026-09-09: Toggling `SHOW_FPS` now creates or removes the FPS label
  immediately on the active desktop/compact screen instead of waiting for the
  next application launch.
- 2026-09-09: The opt-in Windows Annex-B test backend now applies runtime
  volume changes by restarting its ffplay process with the new `-volume`
  parameter. This is explicitly test-backend behavior; board platforms retain
  their own native volume control implementation.
- 2026-09-09: Added bounded seek support to the Windows Annex-B test backend.
  A seek restarts ffplay with an `-ss` offset (raw Annex-B remains a test-only
  backend); `PlatformPlayerSpec` and board behavior are unchanged.
- 2026-09-09: Windows Annex-B test pause/resume now suspends/resumes the
  ffplay process threads. This is isolated to the explicit host test backend;
  board platforms continue to provide pause through their native decoder.
- 2026-09-09: Added persisted `PLAYER_ASPECT` cycling (`default`, `4:3`,
  `16:9`) to desktop Playback Settings and `PlatformPlayerSpec`. The Windows
  Annex-B test backend passes supported aspect modes to ffplay.
- 2026-09-09: Corrected ffplay option ordering so `-aspect` is emitted before
  the raw-H264 input URL, avoiding versions that interpret it as a trailing
  input option.
- 2026-09-09: Corrected `PLAYER_AUTO_FULLSCREEN` timing: desktop fullscreen
  now starts when entering the Player page and is restored when returning to
  detail, rather than forcing the entire application fullscreen at startup.
- 2026-09-09: Validated persisted `PLAYER_ASPECT` on load, accepting only
  `default`, `4:3`, and `16:9`; invalid legacy or manually edited values now
  fall back safely to `default`.
- 2026-09-09: Replaced the player danmaku static multi-line dump with a
  bounded single-track marquee. The most recent message in a five-second
  window is positioned from right to left on each LVGL tick and cleared after
  expiry, avoiding unbounded track objects or extra memory.
- 2026-09-09: Performed a three-pass Windows startup stress check across
  Settings, compact Settings, direct detail, desktop/compact Player, Popular,
  Dynamic, Mine, and Inbox. No early process exits occurred and no wiliwili
  process remained after cleanup.
- 2026-09-09: Added a user-triggered Mine Watch Later removal action using
  `/x/v2/history/toview/del`. Desktop and compact list rows expose Remove only
  for Watch Later entries; accepted responses refresh the bounded list, while
  unsigned sessions do not send a request.
- 2026-09-09: Watch Later Remove now stops LVGL event bubbling so the nested
  action cannot also activate the parent row's open-detail handler.
- 2026-09-09: Bound `PLAYER_AUTO_PLAY` to the completed detail-fetch route:
  when enabled, resolved video detail opens Player automatically; disabling it
  retains the detail page. Explicit player routes remain supported.
- 2026-09-09: Added persisted `DANMAKU_ON` to Other Settings. Disabling it
  prevents fetch/render and clears the active marquee; enabling it during a
  Player session requests the bounded XML slice again.
- 2026-09-09: Added persisted `DANMAKU_STYLE_SPEED` controls to desktop and
  compact Other Settings. The single-track marquee now derives its travel
  duration from 50-200% speed, keeping the animation bounded and adjustable.
- 2026-09-09: Added `platform_player_set_speed` to the player contract. The
  Windows Annex-B test backend restarts ffplay with the selected `setpts`
  multiplier when the speed control changes, so the visible rate and decoder
  behavior stay synchronized.
- 2026-09-09: Extended `PlatformPlayerSpec` with video/audio quality, cache,
  low-quality, and TV-OSD policy fields. The LVGL route now fills these from
  persistent settings so platform adapters can consume one stable contract.
- 2026-09-09: The WBI `playurl` request now consumes the persisted
  `VIDEO_QUALITY` qn value; enabling `PLAYER_LOW_QUALITY` explicitly selects
  qn=16. This removes the previous hard-coded qn=16 behavior from the detail
  to media-source path.
- 2026-09-09: `PLAYER_AUTO_FULLSCREEN` now controls the Windows display
  adapter through the shared platform interface when entering the Player page;
  leaving Player restores windowed mode, while board preview remains fixed at
  800x480.
- 2026-09-09: Windows boundary regression tested invalid H.264 input,
  unbound player, desktop/compact player routes, and direct detail startup;
  each remained alive during the observation window and no stale wiliwili
  processes remained after ten repeated Settings launches.
- 2026-09-09: Player elapsed time now advances from the platform monotonic
  clock while playing, freezes across pause, resumes from the saved offset, and
  resets for a new video. The LVGL position label reflects live playback state.
- 2026-09-09: Player display time and danmaku timing now apply the selected
  `PLAYER_DEFAULT_SPEED` multiplier, keeping progress labels and the marquee
  synchronized with the Windows test backend's playback rate.
- 2026-09-09: Completed a 14-route Windows startup regression pass covering
  desktop/compact Settings, detail, Player, Home/Popular, Dynamic, Mine,
  Inbox, search, history, and search hots. Failures: 0; residual processes: 0.
- 2026-09-09: Closed the `VIDEO_FORMAT`/`VIDEO_CODEC` settings loop. Values
  are persisted with compatibility validation, exposed as cycle controls in
  desktop and compact Settings, carried in `PlatformPlayerSpec`, and applied
  to the WBI `playurl` request (`fnval` and codec selection) instead of being
  silently hard-coded.
- 2026-09-09: Added the original `LIMITED_FPS` setting semantics with
  persisted `0/30/60/120` values. Desktop and compact Settings expose the
  selector, and the LVGL loop now applies the selected frame pacing while
  retaining the unlimited path.
- 2026-09-09: Migrated `HTTP_PROXY` and `HTTP_PROXY_STATUS` beyond storage:
  both settings are editable in desktop/compact Other Settings and are applied
  to every Windows WinHTTP GET/POST session as a named proxy when enabled;
  disabled or empty values return to the system proxy path.
- 2026-09-09: Dynamic feed pagination now preserves the server video offset
  and article page counter. The More control requests the next authenticated
  page and merges results by dynamic ID, so repeated pages do not duplicate
  existing cards.
- 2026-09-09: Replaced the single active danmaku label with three bounded
  scrolling tracks. Each track independently chooses a recent time-window
  item and advances it at the configured speed; the implementation remains
  fixed-size for the board profile instead of allocating an unbounded queue.
- 2026-09-09: Inbox items now retain server-provided message timestamp,
  unread count, and sequence information when available. Desktop and compact
  rows show the timestamp or unread state alongside the existing sender/body.
- 2026-09-09: Added persisted `APP_SWAP_ABXY` and `KEYMAP` controls. The
  former now changes SDL controller A/B mapping between confirm and back at
  runtime; the latter retains the upstream Xbox/PS/keyboard glyph-set choice
  for the forthcoming icon-font renderer.
- 2026-09-09: Added persisted `APP_THEME` cycling (`auto`, `light`, `dark`).
  The Windows presentation adapter applies the light palette at frame submit
  time, including background/panel/text tokens, while retaining the original
  dark palette for auto/dark and preserving the accent color.
- 2026-09-09: Fixed Mine list action parity. Favorites now expose the existing
  authenticated unfavorite operation in both layouts, while Watch Later
  remove buttons correctly call `/x/v2/history/toview/del` rather than the
  unrelated favorites endpoint; both handlers stop row-event bubbling.
- 2026-09-09: Added persisted `IMAGE_REQUEST_THREADS` (`1/2/4/8`) and bound
  the Home/Search cover fetcher to that many asynchronous HTTP jobs per batch.
  Compressed image data is still discarded immediately after LVGL decoding.
- 2026-09-09: The Windows player contract now distinguishes raw Annex-B H.264
  from a normal container stream. Explicit `--h264-test` remains raw-H.264,
  while a resolved Bilibili detail URL starts the Windows decoder directly
  without forcing the incorrect raw-H.264 demuxer.
- 2026-09-09: Runtime regression: `wiliwili_lvgl.exe --player-bv
  BV1qCty6fEPn` remained alive for a 12-second observation window and spawned
  an active ffplay process. Both test PIDs were then terminated explicitly;
  this is evidence of the detail-to-container-decoder launch path, not a
  board-player claim.
- 2026-09-09: Home and video-search feeds now retain a page cursor. More
  requests Popular/Search with incremented API page values and Recommendation
  with an incremented `fresh_idx`, merges BV-unique results, and appends
  decoded covers without invalidating earlier LVGL image descriptors.
- 2026-09-09: Removed the old Search result display cap (8 desktop / 4 compact)
  and converted the result grid to vertical scrolling, so appended pages are
  reachable in both layouts rather than only retained in the data model.
- 2026-09-09: Player Comments now requests eight top-level replies initially
  and exposes More comments. It uses the next WBI reply page, merges replies
  by author/body, then redraws the active Comments panel without duplicating
  existing items.
- 2026-09-09: Added persisted `PLAYER_STRATEGY` (`recommended`, `next`,
  `loop`, `single`) and an end-of-file check for the Windows decoder process.
  It advances a multi-part video first, then follows recommended/next policy,
  loops the current item, or leaves a single item stopped as configured.
- 2026-09-09: Migrated `HTTP_TIMEOUT` and `HTTP_CONNECTION_TIMEOUT` as
  persisted 5/10/15/30-second controls. Every new Windows WinHTTP session
  applies the selected DNS/connect/send and receive bounds through
  `WinHttpSetTimeouts`.
- 2026-09-09: Added an LVGL default focus group across dynamically created
  controls. Keyboard/gamepad Left/Up and Right/Down traverse real focusable
  objects, and Enter/A sends the current control's click event; text fields
  retain their existing typing/submit priority.
- 2026-09-09: Added persisted danmaku `DANMAKU_STYLE_AREA` (25/50/75/100%)
  and `DANMAKU_STYLE_ALPHA` (10-100%). The three-track renderer now hides
  tracks outside the selected area and applies the configured text opacity.
- 2026-09-09: `TLS_VERIFY` is now applied by the Windows network adapter.
  Strict certificate validation remains the default; disabling the setting
  explicitly applies WinHTTP's documented certificate-ignore flags to both
  GET and POST requests.
- 2026-09-09: Replaced stale Mine fallback text with an explicit invalid-tab
  empty state. A ten-route startup regression covered desktop/compact
  Settings, Popular, Dynamic, Mine, Inbox, Search, and History; all stayed
  alive for 1.4 seconds and their temporary processes were terminated.
- 2026-09-09: Added persisted `HISTORY_REPORT`. Playback-start and progress
  reporting route through a single guarded function, so disabling it prevents
  any `/x/v2/history/report` request from the LVGL player.
- 2026-09-09: Inbox chat now keeps the server `msg_seqno` cursor and exposes
  More messages in desktop and compact layouts. Older session pages merge by
  sequence number, while compact Dynamic/Inbox lists scroll vertically once
  they exceed the first viewport.
- 2026-09-09: Bangumi/Cinema search result cards now carry their parsed season
  identifier into an actionable Bilibili season-page route instead of being
  passive text-only cards.
- 2026-09-09: DASH player source parsing now collects the companion audio
  stream in the platform contract. The Windows backend keeps ffplay on one
  video input because ffplay cannot mux two independent inputs; raw Annex-B
  test mode remains single-input by design.
- 2026-09-09: Fixed a Windows regression found during live BV playback
  testing: ffplay cannot accept two independent `-i` inputs, and doing so
  caused duplicate-input parsing followed by `std::bad_alloc`. The backend
  now launches one authenticated/protocol-whitelisted video input. A repeat
  using `--player-bv BV1qCty6fEPn` kept both the LVGL app and ffplay alive at
  5 s and 10 s samples; both test processes were then cleaned up.
- 2026-09-09: Follow-up build fixes the container-mode diagnostic so a missing
  URL is distinguished from a missing raw Annex-B source. The stored DASH
  audio URL is reserved for a future real muxer and is not passed as a second
  ffplay input.
- 2026-09-10: Replaced the Windows container-stream backend with local mpv.
  Bilibili DASH now supplies the video URL as the main input and the parsed
  companion audio URL through mpv's `--audio-file` option, with Referer and
  User-Agent headers, volume, speed, seek, and aspect passed to the same
  backend. Raw Annex-B `--h264-test` remains on ffplay and is intentionally
  separate. Build `C3B43946BFC436EA95A9956DD003A58D` passed a live
  `--player-bv BV1qCty6fEPn` process regression: both LVGL and its new mpv
  child were alive at 5 s and 10 s, and mpv CPU time advanced from 4.59 s to
  24.58 s. The prior ffplay single-video-only observation is superseded for
  normal DASH playback; visual/audio screenshot verification remains pending.
- 2026-09-10: Windows startup regression covered 20 explicit desktop and
  compact/800x480 routes: Popular, video/Bangumi/Cinema search, Dynamic,
  Mine, QR login, Settings, Inbox, History, hot search, detail, and each
  board-preview counterpart. Every route remained alive after the 1.3-second
  initialization observation and each test process was terminated by PID;
  no `wiliwili_lvgl`, mpv, or ffplay process remained. This verifies route
  construction and early lifecycle only. It does not replace visual screenshot
  review, authenticated mutation tests, or end-to-end audio/video validation.
- 2026-09-10: Replaced the former external-browser action for Bangumi/Cinema
  search cards with an internal season route. It requests
  `/pgc/view/web/season`, renders bounded title/description/episode data in
  desktop and 800x480 layouts, returns to the originating media-search page,
  and opens an episode through the existing BV detail/player route. The
  direct `--season <season-id>` entry mirrors the upstream season Intent for
  regression. The public PGC ranking endpoint supplied valid season `109700`;
  its season response contains 73 episodes with BV/CID fields. Build
  `D5B36AF12A7CBB0965A27B2AD56B3E3E` kept `--season 109700` alive for eight
  seconds before explicit PID cleanup.
  Visual review is still pending because the installed Windows UI automation
  service fails before enumeration with a missing kernel-assets path.
- 2026-09-10: Fixed the season navigation stack: selecting an internal episode
  marks its detail origin, so Back now returns to the retained season episode
  list instead of incorrectly returning to Home. Related-video and ordinary
  card openings explicitly clear that origin. Build
  `262D2A4EA2DF399A6F4EBEBF655857B0` passed a repeat `--season 109700`
  seven-second lifecycle check with explicit cleanup.
- 2026-09-10: Fixed two Windows runtime regressions. QR polling now checks
  Bilibili's nested `data.code` rather than the transport envelope `code`,
  which is normally zero even while a QR token is waiting and previously made
  the first poll terminate the login flow. The mpv container backend now
  converts the persisted percentage speed to a multiplier (`100 -> 1.00`,
  `125 -> 1.25`) instead of issuing `--speed=100`. Build
  `B1D844B817FCB4DFBDDB2E82D4EC1E26` passed QR page lifecycle startup; the
  live BV-player command-line regression independently confirmed
  `--speed=1.00`.
