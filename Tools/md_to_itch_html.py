#!/usr/bin/env python3
"""Turn a docs/ship/itch-*.md page draft into basic HTML for the itch.io description editor.

itch.io's editor takes simple HTML: headings, paragraphs, bold, italics, links, lists and inline code. It doesn't take
Markdown, and tables don't survive it reliably, so tables become bullet lists ("Action: keys / pad").
The page header (title, tags, platform, download, video lines before the first ---) and HTML comments are left out:
those go into itch's own fields. Usage: python3 Tools/md_to_itch_html.py docs/ship/itch-megajam.md > out.html
"""
import html, re, sys

def inline(t):
    t = html.escape(t, quote=False)
    t = re.sub(r'`([^`]+)`', r'<code>\1</code>', t)
    t = re.sub(r'\*\*([^*]+)\*\*', r'<strong>\1</strong>', t)
    t = re.sub(r'(?<!\*)\*([^*]+)\*(?!\*)', r'<em>\1</em>', t)
    t = re.sub(r'\[([^\]]+)\]\((https?://[^)]+)\)', r'<a href="\2">\1</a>', t)
    return t

def convert(src):
    src = re.sub(r'<!--.*?-->', '', src, flags=re.S)
    body = src.split('\n---\n', 1)[1] if '\n---\n' in src else src
    out, para, lst, table = [], [], None, []
    def flush_para():
        if para:
            out.append('<p>' + inline(' '.join(para)) + '</p>')
            para.clear()
    def flush_list():
        nonlocal lst
        if lst:
            tag, items = lst
            out.append('<%s>\n%s\n</%s>' % (tag, '\n'.join('<li>%s</li>' % inline(i) for i in items), tag))
            lst = None
    def flush_table():
        if table:
            rows = [[c.strip() for c in r.strip().strip('|').split('|')] for r in table]
            head, data = rows[0], [r for r in rows[1:] if not set(''.join(r)) <= set('-: ')]
            items = []
            for r in data:
                if len(r) == 3 and 'pad' in head[2].lower():
                    items.append(f'**{r[0]}**: {r[1]} (pad: {r[2]})')   # the controls table
                else:
                    items.append(f'**{r[0]}**: ' + '; '.join(r[1:]))
            out.append('<ul>\n%s\n</ul>' % '\n'.join('<li>%s</li>' % inline(i) for i in items))
            table.clear()
    for line in body.split('\n'):
        s = line.rstrip()
        if s.startswith('|'):
            flush_para(); flush_list(); table.append(s); continue
        flush_table()
        m = re.match(r'^(#{1,3}) (.*)', s)
        if m:
            flush_para(); flush_list()
            level = max(len(m.group(1)), 2)  # the page title is itch's own field; sections are h2
            out.append('<h%d>%s</h%d>' % (level, inline(m.group(2)), level))
            continue
        m = re.match(r'^(\d+)\. (.*)', s)
        if m:
            flush_para()
            if not lst or lst[0] != 'ol': flush_list(); lst = ('ol', [])
            lst[1].append(m.group(2)); continue
        m = re.match(r'^- (.*)', s)
        if m:
            flush_para()
            if not lst or lst[0] != 'ul': flush_list(); lst = ('ul', [])
            lst[1].append(m.group(1)); continue
        if not s.strip():
            flush_para(); flush_list(); continue
        flush_list(); para.append(s.strip())
    flush_para(); flush_list(); flush_table()
    return '\n\n'.join(out) + '\n'

if __name__ == '__main__':
    sys.stdout.write(convert(open(sys.argv[1], encoding='utf-8').read()))
