"""Writes THIRD_PARTY_LICENSES.txt: the licenses of the libraries that are compiled into the plugin, copied from the JUCE
checkout the plugin was built with.
usage: python tools/make_third_party.py <JUCE folder>"""
import sys
from pathlib import Path

J = Path(sys.argv[1]) / 'modules'; ROOT = Path(__file__).resolve().parent.parent
PARTS = [
    ('JUCE framework', 'https://github.com/juce-framework/JUCE', None,
     'JUCE 9 is used under the GNU Affero General Public License, version 3 (the same text as the file LICENSE of Tobo Imager).\n'
     'Copyright (c) Raw Material Software Limited.'),
    ('VST 3 SDK', 'https://github.com/steinbergmedia/vst3sdk', 'juce_audio_processors_headless/format_types/VST3_SDK/LICENSE.txt',
     'VST is a registered trademark of Steinberg Media Technologies GmbH.'),
    ('zlib', 'https://zlib.net/', 'juce_core/zip/zlib/LICENSE', None),
    ('libpng', 'http://www.libpng.org/', 'juce_graphics/image_formats/pnglib/LICENSE', None),
    ('The Independent JPEG Group\'s JPEG software', 'https://www.ijg.org/', ('juce_graphics/image_formats/jpglib/README', 'LEGAL ISSUES\n============', 'REFERENCES\n=========='),
     'This software is based in part on the work of the Independent JPEG Group.'),
    ('HarfBuzz', 'https://github.com/harfbuzz/harfbuzz', 'juce_graphics/fonts/harfbuzz/COPYING', None),
    ('SheenBidi', 'https://github.com/Tehreer/SheenBidi', 'juce_graphics/unicode/sheenbidi/LICENSE', None),
    ('LunaSVG', 'https://github.com/sammycage/lunasvg', 'juce_graphics/drawables/lunasvg/LICENSE', None),
    ('PlutoVG', 'https://github.com/sammycage/plutovg', 'juce_graphics/drawables/lunasvg/plutovg/LICENSE', None),
    ('FLAC', 'https://xiph.org/flac/', 'juce_audio_formats/codecs/flac/Flac Licence.txt', None),
    ('libogg', 'https://xiph.org/ogg/', 'juce_audio_formats/codecs/ogg/COPYING', None),
    ('libvorbis', 'https://xiph.org/vorbis/', 'juce_audio_formats/codecs/vorbis/COPYING', None),
]


def main():
    out = ['Tobo Imager: licenses of the third-party software in the plugin binary', '=' * 72, '',
           'The plugin is built with the JUCE framework. The libraries below are part of JUCE\'s modules and are compiled into',
           'the binary together with it. Tobo Imager itself is under the GNU AGPL, version 3 or later (see LICENSE).', '']
    for name, url, src, note in PARTS:
        out += ['', '-' * 72, name, url, '-' * 72, '']
        if note: out += [note, '']
        if isinstance(src, tuple):
            t = (J / src[0]).read_text(encoding='utf-8', errors='replace'); i = t.index(src[1]); out.append(t[i:t.index(src[2], i)].rstrip())
        elif src:
            out.append((J / src).read_text(encoding='utf-8', errors='replace').rstrip())
    (ROOT / 'THIRD_PARTY_LICENSES.txt').write_text('\n'.join(out) + '\n', encoding='utf-8', newline='\n')
    print('written', len(out), 'blocks of lines')


if __name__ == '__main__':
    main()
