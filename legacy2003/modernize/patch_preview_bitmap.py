#!/usr/bin/env python3
"""Preserve source aspect ratio for model portraits and HUD thumbnail panels."""
from pathlib import Path
p=Path(__file__).resolve().parents[1]/'gameui/BitmapImagePanel.cpp'
raw=p.read_bytes();crlf=b'\r\n' in raw
s=raw.decode('latin1').replace('\r\n','\n')
if 'HL2MODERN_PREVIEW_ASPECT' not in s:
    before='''	surface()->DrawSetTexture( m_nTextureId );
	surface()->DrawTexturedRect( 0, 0, w, h );'''
    after='''	surface()->DrawSetTexture( m_nTextureId );
	// HL2MODERN_PREVIEW_ASPECT: fit the complete image without stretching it.
	int imageW=0, imageH=0;
	surface()->DrawGetTextureSize(m_nTextureId,imageW,imageH);
	if (imageW>0 && imageH>0 && w>0 && h>0)
	{
		const float scaleX=(float)w/(float)imageW;
		const float scaleY=(float)h/(float)imageH;
		const float scale=scaleX<scaleY?scaleX:scaleY;
		const int outW=(int)(imageW*scale);
		const int outH=(int)(imageH*scale);
		surface()->DrawTexturedRect((w-outW)/2,(h-outH)/2,(w+outW)/2,(h+outH)/2);
	}'''
    assert s.count(before)==1
    s=s.replace(before,after,1)
    s=s.replace('strcpy( m_szTexture, filename );','''strncpy(m_szTexture, filename, sizeof(m_szTexture)-1);
	m_szTexture[sizeof(m_szTexture)-1]=0;''')
    s=s.replace('\tif ( m_bUploaded )\n\t{\n\t\tforceReload();', '\tif (!m_szTexture[0]) return;\n\tif ( m_bUploaded )\n\t{\n\t\tforceReload();', 1)
    p.write_bytes((s.replace('\n','\r\n') if crlf else s).encode('latin1'))
print('Aspect-correct preview images patched.')
