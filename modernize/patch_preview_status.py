#!/usr/bin/env python3
"""Make the real-time portrait status report absence rather than imply 3D rendering."""
from pathlib import Path
path=Path(__file__).resolve().parents[1]/'gameui/OptionsSubMultiplayer.cpp'
raw=path.read_bytes();crlf=b'\r\n' in raw
src=raw.decode('latin1').replace('\r\n','\n')
# v18 renders real studio geometry; the old portrait transform is superseded.
if 'HL2MODERN_ANIMATED_PREVIEW' in src:
    print('Animated model preview already supplies asset status.')
    raise SystemExit(0)
old='''		if(entry && m_pModelImage)
		{
			char material[160], asset[190];'''
new='''		bool hasPortrait = false; // HL2MODERN_PREVIEW_ASSET_STATUS
		if(entry && m_pModelImage)
		{
			char material[160], asset[190];'''
assert old in src or 'HL2MODERN_PREVIEW_ASSET_STATUS' in src
if 'HL2MODERN_PREVIEW_ASSET_STATUS' not in src:
    src=src.replace(old,new,1)
    old='''			if(filesystem()->FileExists(asset))
			{
				m_pModelImage->SetVisible(true);'''
    new='''			if(filesystem()->FileExists(asset))
			{
				hasPortrait = true;
				m_pModelImage->SetVisible(true);'''
    assert old in src
    src=src.replace(old,new,1)
    old='''			m_pPreviewLabel->SetText(entry ? entry->label : "No compatible player model");'''
    new='''			// The original UI has a 2D bitmap panel. Show a live portrait if
			// installed, but never pretend an absent image is a 3D preview.
			char status[200];
			if (!entry)
				m_pPreviewLabel->SetText("No compatible player model");
			else if (hasPortrait)
				m_pPreviewLabel->SetText(entry->label);
			else
			{
				_snprintf(status,sizeof(status)-1,"%s (portrait unavailable)",entry->label);
				status[sizeof(status)-1]=0;
				m_pPreviewLabel->SetText(status);
			}'''
    assert old in src
    src=src.replace(old,new,1)
    path.write_bytes((src.replace('\n','\r\n') if crlf else src).encode('latin1'))
print('Player-model selection reports installed portrait availability.')
