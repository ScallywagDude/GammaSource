"""Let the 2003 VTF loader read files that store fewer mips than a full chain
(NOMIP sprites) and 7.2 files whose header is larger than VTFFileHeader_t."""
from pathlib import Path
p = Path(r"C:\Users\doge6\Videos\GammaSource-main\vtf\vtf.cpp")
d = p.read_bytes()
nl = b"\r\n" if b"\r\n" in d else b"\n"
L = lambda *lines: nl.join(lines)

edits = [
    # members
    (L(b"\t// FIXME: Remove", b"\t// This is to make sure old-format .vtf files are read properly", b"\tint\tm_pVersion[2];", b"};"),
     L(b"\t// FIXME: Remove", b"\t// This is to make sure old-format .vtf files are read properly", b"\tint\tm_pVersion[2];", b"",
       b"\t// On-disk layout: header size (7.2+ headers are larger than VTFFileHeader_t)",
       b"\t// and how many mip levels are actually stored (NOMIP files store one).",
       b"\tint m_nFileHeaderSize;", b"\tint m_nFileMipCount;", b"};")),
    # constructor
    (L(b"\tm_iStartFrame = 0;", b"\tm_flBumpScale = 1.0f;"),
     L(b"\tm_iStartFrame = 0;", b"\tm_nFileHeaderSize = VTFFileHeaderSize();", b"\tm_nFileMipCount = 0;", b"\tm_flBumpScale = 1.0f;")),
    # low-res start
    (b"\t*pStartLocation = VTFFileHeaderSize();",
     b"\t*pStartLocation = m_nFileHeaderSize;"),
    # file size counts only stored mips
    (L(b"\tint nFaceSize = ComputeFaceSize( nMipSkipCount );"),
     L(b"\tint nFaceSize = ComputeFaceSize( nMipSkipCount );",
       b"\tif ( m_nFileMipCount < m_nMipCount )",
       b"\t{",
       b"\t\t// Mip levels the file doesn't store aren't read (see LoadImageData).",
       b"\t\tint nStart = ( nMipSkipCount > m_nFileMipCount ) ? nMipSkipCount : m_nFileMipCount;",
       b"\t\tnFaceSize -= ComputeFaceSize( nStart );",
       b"\t}")),
    # unserialize: record layout, skip extended header
    (L(b"\tm_pVersion[0] = header.version[0];", b"\tm_pVersion[1] = header.version[1];"),
     L(b"\tm_pVersion[0] = header.version[0];", b"\tm_pVersion[1] = header.version[1];", b"",
       b"\tm_nFileHeaderSize = VTFFileHeaderSize();",
       b"\tif ( header.version[1] >= 2 && header.headerSize > VTFFileHeaderSize() && header.headerSize <= 4096 )",
       b"\t\tm_nFileHeaderSize = header.headerSize;",
       b"\tm_nFileMipCount = m_nMipCount;",
       b"\tif ( header.numMipLevels > 0 && header.numMipLevels < m_nMipCount )",
       b"\t\tm_nFileMipCount = header.numMipLevels;")),
    (L(b"\tif (!LoadLowResData( buf ))", b"\t\treturn false;"),
     L(b"\tif ( m_nFileHeaderSize > VTFFileHeaderSize() )",
       b"\t\tbuf.SeekGet( CUtlBuffer::SEEK_HEAD, m_nFileHeaderSize );",
       b"",
       b"\tif (!LoadLowResData( buf ))", b"\t\treturn false;")),
]
for old, new in edits:
    n = d.count(old)
    assert n == 1, (n, old)
    d = d.replace(old, new)
p.write_bytes(d)
print("ok")
