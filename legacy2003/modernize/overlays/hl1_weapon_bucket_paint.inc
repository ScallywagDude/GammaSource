    // HL2MODERN_HL1_BUCKETS: original HL1 arrangement, existing 2002 HUD palette.
    // Reference behavior: ValveSoftware/halflife cl_dll/ammo.cpp DrawWList.
    if (!ShouldDraw()) return;
    C_BasePlayer *player=C_BasePlayer::GetLocalPlayer();
    if (!player) return;
    C_BaseCombatWeapon *selected=GetSelectedWeapon();
    if (m_hLastPickedUpWeapon) selected=m_hLastPickedUpWeapon;
    if (!selected) return;
    const int active=selected->GetSlot();
    if (active<0 || active>=MAX_WEAPON_SLOTS) return;
    int count=0,selectedRow=0;
    for (int pos=0;pos<MAX_WEAPON_POSITIONS;++pos)
    {
        C_BaseCombatWeapon *weapon=GetWeaponInSlot(active,pos);
        if (!weapon) continue;
        if (weapon==selected) selectedRow=count;
        ++count;
    }
    HL2ModernHud::BucketLayout layout=HL2ModernHud::WeaponBuckets(GetWide(),GetTall(),MAX_WEAPON_SLOTS,count,selectedRow);
    Color normal=gHUD.m_clrNormal;
    float alpha=m_flSelectionAlphaOverride;
    if (alpha<0) alpha=0;
    if (alpha>255) alpha=255;
    for (int slot=0;slot<MAX_WEAPON_SLOTS;++slot)
    {
        const int x=HL2ModernHud::WeaponBucketX(layout,slot,active);
        const int wide=slot==active?layout.expanded:layout.compact;
        Color header=normal;
        header[3]=(int)(alpha*(slot==active?0.32f:0.12f));
        DrawBox(x,layout.y,wide,layout.header,header,255,slot+1);
        surface()->DrawSetColor(normal[0],normal[1],normal[2],(int)alpha);
        surface()->DrawFilledRect(x,layout.y+layout.header-2,x+wide,layout.y+layout.header);
        int ordinal=0;
        for (int pos=0;pos<MAX_WEAPON_POSITIONS;++pos)
        {
            C_BaseCombatWeapon *pWeapon=GetWeaponInSlot(slot,pos);
            if (!pWeapon) continue;
            Color col=pWeapon->HasAnyAmmo()?normal:gHUD.m_clrCaution;
            const bool isSelected=pWeapon==selected;
            col[3]=(int)(alpha*(isSelected?1.0f:0.7f));
            if (slot!=active)
            {
                const int y=layout.y+layout.header+layout.gap+ordinal*10;
                if (y+6<GetTall())
                {
                    surface()->DrawSetColor(col);
                    surface()->DrawFilledRect(x,y,x+wide,y+6);
                }
                ++ordinal;
                continue;
            }
            const int row=ordinal++;
            if (row<layout.first || row>=layout.first+layout.visible) continue;
            const int y=layout.y+layout.header+layout.gap+(row-layout.first)*(layout.row+layout.gap);
            DrawBox(x,y,wide,layout.row,Color(0,0,0,isSelected?128:64),alpha,-1);
            if (isSelected)
            {
                surface()->DrawSetColor(col);
                surface()->DrawOutlinedRect(x,y,x+wide,y+layout.row);
                surface()->DrawFilledRect(x,y,x+3,y+layout.row);
            }
            // HL2MODERN_WEAPON_HUD_FIT: every original script icon uses the same bounded canvas.
            const CHudTexture *sprite=isSelected?pWeapon->GetSpriteActive():pWeapon->GetSpriteInactive();
            if (!sprite) sprite=pWeapon->GetSpriteActive();
            if (!sprite) sprite=pWeapon->GetSpriteInactive();
            bool bIconDrawn = false; // HL2MODERN_MISSING_WEAPON_ICON
            if (sprite)
            {
                HL2ModernHud::Rect fitted;
                if (HL2ModernHud::FitIcon(sprite->Width(),sprite->Height(),x+6,y+3,wide-12,layout.row-20,fitted))
                {
                    sprite->DrawSelf(fitted.x,fitted.y,fitted.w,fitted.h,col);
                    bIconDrawn = true;
                }
            }
            if (!bIconDrawn)
            {
                surface()->DrawSetTextColor(col);
                surface()->DrawSetTextFont(m_hTextFont);
                surface()->DrawSetTextPos(x+8,y+5);
                const char *fallback = "WPN";
                for (const char *c=fallback;*c;++c) surface()->DrawUnicodeChar((wchar_t)*c);
            }
            surface()->DrawSetTextColor(col);
            surface()->DrawSetTextFont(m_hTextFont);
            surface()->DrawSetTextPos(x+6,y+layout.row-15);
            // HL2MODERN_BUCKET_LOCALIZATION: retain localized names and bounded fallback.
            const char *name=pWeapon->GetWpnData().szPrintName;
            wchar_t converted[128];
            const wchar_t *label=vgui::localize()->Find(name);
            if (!label)
            {
                vgui::localize()->ConvertANSIToUnicode(name,converted,sizeof(converted));
                converted[127]=0;
                label=converted;
            }
            int textWidth=0;
            for (const wchar_t *c=label;*c;++c)
            {
                int characterWidth=surface()->GetCharacterWidth(m_hTextFont,*c);
                if (textWidth+characterWidth>wide-12) break;
                surface()->DrawUnicodeChar(*c);
                textWidth+=characterWidth;
            }
        }
    }
