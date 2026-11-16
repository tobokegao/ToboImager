"""Makes README.md, README_ja.md, docs/*.md and the two PDF manuals from manual/manual_en.md and manual/manual_ja.md.
The version is read from CMakeLists.txt; the date of the release is given on the command line.
  python tools/make_docs.py 2026-11-16
Needs the Python package "markdown" and Google Chrome (for the PDF files)."""
import re, subprocess, sys
from pathlib import Path
import markdown

ROOT = Path(__file__).resolve().parent.parent
CHROME = r'C:\Program Files\Google\Chrome\Application\chrome.exe'
NOTE_EN = '<!-- Generated from manual/manual_en.md by tools/make_docs.py. Change the source, not this file. -->'
NOTE_JA = '<!-- manual/manual_ja.md から tools/make_docs.py が作るファイル。直すときは原稿を直して作り直す -->'
CSS = '''
@page { size: A4; margin: 16mm 15mm; }
body { font-family: "Segoe UI", "Yu Gothic UI", "Meiryo", sans-serif; font-size: 10.5pt; line-height: 1.65; color: #0e1e30; }
h1 { font-size: 19pt; margin: 0 0 8pt; }
h2 { font-size: 14.5pt; margin: 20pt 0 6pt; padding-bottom: 3pt; border-bottom: 1.5pt solid #0e1e30; break-after: avoid; }
h3 { font-size: 12pt; margin: 14pt 0 4pt; break-after: avoid; }
img { max-width: 100%; display: block; margin: 8pt auto; }
table { border-collapse: collapse; width: 100%; margin: 8pt 0; font-size: 9.5pt; }
th, td { border: 0.75pt solid #95a7b3; padding: 4pt 6pt; vertical-align: top; text-align: left; }
th { background: #e4e9ef; }
th:first-child, td:first-child { white-space: nowrap; }
/* the middle column of a table of three (the name of a part, the range of a parameter) is kept on one line */
th:first-child:nth-last-child(3) + th, td:first-child:nth-last-child(3) + td { white-space: nowrap; }
tr { break-inside: avoid; }
code { font-family: Consolas, monospace; font-size: 9.5pt; background: #eef1f5; padding: 0 2pt; }
pre { background: #eef1f5; padding: 7pt 9pt; font-size: 9pt; line-height: 1.4; white-space: pre-wrap; }
pre code { background: none; padding: 0; }
a { color: #1c5fa8; text-decoration: none; }
.ver { color: #55687a; margin: 0 0 10pt; }
'''


def source(lang, date):
    ver = re.search(r'project\(ToboImager VERSION ([0-9.]+)\)', (ROOT / 'CMakeLists.txt').read_text(encoding='utf-8')).group(1)
    return (ROOT / 'manual' / f'manual_{lang}.md').read_text(encoding='utf-8').replace('{VERSION}', ver).replace('{DATE}', date), ver


def slug(title): return re.sub(r'[^a-z0-9]+', '-', title.lower()).strip('-')


def write(path, text): path.write_text(text.rstrip() + '\n', encoding='utf-8', newline='\n'); print('made', path.relative_to(ROOT))


def pdf(md, ver, date, name, lang):
    lines = md.split('\n'); title = lines[0][2:]
    body = markdown.markdown('\n'.join(lines[1:]), extensions=['tables', 'fenced_code'])
    body = body.replace('src="docs/img/', 'src="' + (ROOT / 'docs' / 'img').as_uri() + '/')
    body = re.sub(r'href="(?!https?:|#)([^"]+)"', r'href="https://github.com/tobokegao/ToboImager/blob/main/"', body)      # no local paths in the PDF
    html = f'<!doctype html><html lang="{lang}"><head><meta charset="utf-8"><title>{title}</title><style>{CSS}</style></head><body><h1>{title}</h1><p class="ver">Version {ver} ({date})</p>{body}</body></html>'
    tmp = ROOT / 'build_manual.html'; tmp.write_text(html, encoding='utf-8')
    subprocess.run([CHROME, '--headless', '--disable-gpu', '--no-pdf-header-footer', f'--print-to-pdf={ROOT / name}', tmp.as_uri()], check=True, capture_output=True)
    tmp.unlink(); print('made', name)


def main():
    date = sys.argv[1]
    en, ver = source('en', date); ja, _ = source('ja', date)

    # ---- English: README.md (the introduction and the list of pages) and one page per section in docs/ ----
    parts = re.split(r'\n(?=## )', en); intro = parts[0]; secs = [(p.split('\n', 1)[0][3:], p) for p in parts[1:]]
    docs = ROOT / 'docs'
    for f in docs.glob('*.md'): f.unlink()
    readme = [intro.rstrip(), '', NOTE_EN, '',
              '**Download:** https://github.com/tobokegao/ToboImager/releases/latest  ',
              '**Demo video:** [tobo_imager_demo.mp4](https://github.com/tobokegao/ToboImager/releases/latest/download/tobo_imager_demo.mp4) (with the plugin switched off and on every four bars across three sounds)  ',
              '**日本語:** [README_ja.md](README_ja.md)', '',
              'The full manual is also in the download as a PDF (`ToboImager_manual_en.pdf`; `ToboImager_manual.pdf` is the Japanese version).', '',
              '## Documentation', '']
    for i, (t, p) in enumerate(secs):
        readme.append(f'- [{t}](docs/{slug(t)}.md)')
        nav = []
        if i > 0: nav.append(f'← [{secs[i - 1][0]}]({slug(secs[i - 1][0])}.md)')
        nav.append('[Contents](../README.md#documentation)')
        if i + 1 < len(secs): nav.append(f'[{secs[i + 1][0]}]({slug(secs[i + 1][0])}.md) →')
        page = f'[Tobo Imager](../README.md) › {t}\n\n{NOTE_EN}\n\n' + p.replace('](docs/img/', '](img/').rstrip() + '\n\n---\n\n' + '   |   '.join(nav)
        write(docs / f'{slug(t)}.md', page)
    readme += ['', '## License', '', 'AGPL-3.0-or-later. See `LICENSE`, `CREDITS.md` and `THIRD_PARTY_LICENSES.txt`.']
    write(ROOT / 'README.md', '\n'.join(readme))

    # ---- Japanese: the whole manual as one page ----
    jl = ja.split('\n')
    links = ['**ダウンロード:** https://github.com/tobokegao/ToboImager/releases/latest  ',
             '**デモ動画:** [tobo_imager_demo.mp4](https://github.com/tobokegao/ToboImager/releases/latest/download/tobo_imager_demo.mp4)（3種類の音で、4小節ごとにプラグインをオフ・オンしています）  ',
             '**English:** [README.md](README.md)', '',
             '説明書はダウンロードしたZIPファイルにもPDFで入っています（日本語版は`ToboImager_manual.pdf`、英語版は`ToboImager_manual_en.pdf`）。', '']
    k = next(i for i, l in enumerate(jl) if l.startswith('## '))
    write(ROOT / 'README_ja.md', '\n'.join([jl[0], '', NOTE_JA] + jl[1:k] + links + jl[k:]))

    pdf(en, ver, date, 'ToboImager_manual_en.pdf', 'en'); pdf(ja, ver, date, 'ToboImager_manual.pdf', 'ja')


if __name__ == '__main__':
    main()
