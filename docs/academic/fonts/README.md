# Presentation typography

Vazirmatn **33.003**, unmodified Regular/Bold WOFF2 files, is bundled for offline
Persian/Arabic document rendering. It is licensed under SIL OFL-1.1; see OFL.txt.
Source: [official release](https://github.com/rastikerdar/vazirmatn/releases/tag/v33.003),
[web fonts](https://github.com/rastikerdar/vazirmatn/tree/v33.003/fonts/webfonts).

SHA-256:

```text
e382101336c6eb32cfb31381c027d02d2e0354bad08f6a395d4088beb3db3d91  Vazirmatn-Regular.woff2
836fae7d42d83faa249bc00e0099592be98a1fa260d22d82f269b6091e585627  Vazirmatn-Bold.woff2
b69fd4c680b8f3f225feabcc655a2c585d97627b8f5f5c0f9985e894069f3a56  Vazirmatn-Regular.ttf
```

Latin text and code prefer the system's Consolas font. The presentation's embedded
Vazirmatn face supplies Persian glyphs; the PDF embeds the glyphs actually used.
No proprietary Consolas font file is redistributed. These are document assets;
the application's English Inter/JetBrains Mono interface is unchanged.
The unmodified Regular TTF is used for standalone Matplotlib figures; WOFF2 faces
are embedded in exported browser HTML for reliable offline loading.
