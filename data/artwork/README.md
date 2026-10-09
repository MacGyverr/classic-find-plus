# Application artwork

User-supplied originals moved from `src/`:
- `favicon.ico`: original 16×16 pixel avatar, retained unchanged.
- `about.png`: original 320×320 portrait, formerly `src/me.png`, retained unchanged.

`icon.png` is the 16×16 PNG equivalent of `favicon.ico` for Linux icon themes.
It preserves the decoded pixels and transparency without resizing or redrawing.
To regenerate it with the installed GdkPixbuf utility:

```sh
gdk-pixbuf-thumbnailer -s 16 data/artwork/favicon.ico data/artwork/icon.png
```

`icon.png` and `about.png` are embedded in the app as GResources. The small icon
is also installed as `hicolor/16x16/apps/classic-find-plus.png`. Help/About scales
the portrait to 160×160 for display; the original embedded bytes stay unchanged.
Find Now keeps the native system-search icon. Windows download metadata is
ignored and is not bundled in the application.
