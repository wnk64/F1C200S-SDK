# Windows LVGL Migration Map

This worktree replaces rendering only. The upstream `yoga` branch at
`88e5876bea9502d06f46a8656e3530684d3aaf7d` remains the behavior contract.

## Implemented vertical slice

| Upstream contract | LVGL status | Validation requirement |
| --- | --- | --- |
| `activity/main.xml` shell navigation | Partial: visual shell only | Windows capture |
| `fragment/home_tab.xml` top tabs | Partial: visual tabs only | Windows capture |
| `fragment/home_hots_all.cpp` public Popular data | Implemented: WinHTTP fetch, real data cards, click opens original Bilibili video page | Windows capture plus browser URL |
| `fragment/home_recommends.cpp` WBI recommend data | Pending | WBI signing port + visible cards |

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
