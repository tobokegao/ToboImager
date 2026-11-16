"""Puts the download together: release/ToboImager_<version>_win64.zip with the plugin (from build/), the two PDF manuals,
the licenses and a short README.txt. Run after the build and after tools/make_docs.py.
usage: python tools/make_release.py"""
import re, zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
VST = ROOT / 'build' / 'ToboImager_artefacts' / 'Release' / 'VST3' / 'Tobo Imager.vst3'
README = r'''Tobo Imager {ver} - stereo imager with one to four bands and a mono-compatible stereoizer (VST3, 64-bit Windows)
https://github.com/tobokegao/ToboImager

Install: copy the folder "Tobo Imager.vst3" (the whole folder) to
    C:\Program Files\Common Files\VST3\
and let your DAW scan for new plugins.
The manual is ToboImager_manual_en.pdf.

インストール: 「Tobo Imager.vst3」フォルダーを、フォルダーごと
    C:\Program Files\Common Files\VST3\
にコピーして、DAWでプラグインを再スキャンしてください。
説明書はToboImager_manual.pdfです。

License: GNU AGPL version 3 or later (LICENSE). The source code is at the address above.
Third-party licenses: THIRD_PARTY_LICENSES.txt. Credits: CREDITS.md.
VST is a registered trademark of Steinberg Media Technologies GmbH.
'''


def main():
    ver = re.search(r'project\(ToboImager VERSION ([0-9.]+)\)', (ROOT / 'CMakeLists.txt').read_text(encoding='utf-8')).group(1)
    out = ROOT / 'release'; out.mkdir(exist_ok=True); z = out / f'ToboImager_{ver}_win64.zip'
    with zipfile.ZipFile(z, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as f:
        for p in sorted(VST.rglob('*')):
            if p.is_file(): f.write(p, (Path('Tobo Imager.vst3') / p.relative_to(VST)).as_posix())
        for n in ('ToboImager_manual_en.pdf', 'ToboImager_manual.pdf', 'LICENSE', 'CREDITS.md', 'THIRD_PARTY_LICENSES.txt'): f.write(ROOT / n, n)
        f.writestr('README.txt', README.format(ver=ver).replace('\n', '\r\n'))
    print(z, z.stat().st_size, 'bytes'); print('\n'.join(zipfile.ZipFile(z).namelist()))


if __name__ == '__main__':
    main()
