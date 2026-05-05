# Reva Player v1.0.0

Initial release for Reva Player.

## Highlights

- Local video and audio playback through libmpv.
- Playlists, saved folder lists, history, resume state, bookmarks, thumbnails, and hover preview.
- Battery, Balanced, and Quality playback profiles.
- Private bundled mpv script support, including Reva's packaged thumbfast integration.
- Linux desktop metadata, icons, and AppImage, DEB, and RPM packaging scripts.

## Performance

- Playlist presentation refreshes avoid repeated canonical path resolution in UI paths.
- Playlist filesystem details are cached and invalidated by real playlist/source events.
- Fullscreen chrome restoration is deferred once per transition to reduce layout churn.

## Packages

- AppImage: `Reva-Player-1.0.0-x86_64.AppImage`
- DEB: `revaplayer_1.0.0_amd64.deb`
- RPM: `revaplayer-1.0.0-1.x86_64.rpm`
