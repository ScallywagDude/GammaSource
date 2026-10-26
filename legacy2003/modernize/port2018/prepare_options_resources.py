"""Generate 2018-compatible options resources using GammaSource's 2003 scheme."""
from pathlib import Path
import json,shutil
REPO=Path(r'C:\Users\doge6\Videos\GammaSource-main')
OUT=REPO/'gammasource_content_2018/resource'
BACKUP=Path(r'C:\Users\doge6\Documents\GammaSourceTest\handoff_finish_backups\repo')
PAGES={}
def ctl(kind,name,x,y,w,h=24,text=None,**extra):
 d={'ControlName':kind,'fieldName':name,'xpos':x,'ypos':y,'wide':w,'tall':h,'visible':1,'enabled':1,'tabPosition':0}
 if text is not None:d['labelText']=text
 d.update(extra);return name,d
def label(name,text,x,y,w=210,h=24,**kw):return ctl('Label',name,x,y,w,h,text,textAlignment='west',**kw)
def combo(name,x,y,w=210):return ctl('ComboBox',name,x,y,w,24,editable=0)
def slider(name,x,y,w=210,h=40):return ctl('CCvarSlider',name,x,y,w,h,leftText='',rightText='')
def check(name,text,x,y,w=220,kind='CCvarToggleCheckButton',**kw):return ctl(kind,name,x,y,w,24,text,**kw)
def button(name,text,x,y,w=110,cmd=None):
 kw={'command':cmd} if cmd else {};return ctl('Button',name,x,y,w,24,text,**kw)
def page(file,items):PAGES[file]=dict(items)
page('OptionsSubKeyboard.res',[
 ctl('SectionedListPanel','listpanel_keybindlist',12,10,480,252),
 button('Defaults','Use defaults',12,274,110,'Defaults'),button('KeyAdvancedButton','Advanced keys...',130,274,124,'Advanced'),
 button('ChangeKeyButton','Edit key',310,274,84,'ChangeKey'),button('ClearKeyButton','Clear key',404,274,84,'ClearKey')])
page('OptionsSubMouse.res',[
 check('ReverseMouse','Reverse mouse',20,10,kind='CCvarNegateCheckButton'),check('MouseFilter','Mouse filter',260,10),
 label('Label3','Mouse sensitivity',24,48,160),slider('Slider',184,46,224),ctl('TextEntry','SensitivityLabel',420,46,60,24,NumericInputOnly=1),
 check('MouseRaw','Raw mouse input',20,96),check('MouseAccelerationCheckbox','Mouse acceleration',260,96,kind='CheckButton'),
 label('AccelLabel','Acceleration exponent',24,130,160),slider('MouseAccelerationSlider',184,126,224),ctl('TextEntry','MouseAccelerationLabel',420,126,60,24,NumericInputOnly=1),
 check('HudQuickInfo','Quick weapon info',20,172),ctl('Divider','MouseDivider',24,202,456,2),
 check('Joystick','Enable joystick',20,210),check('ReverseJoystick','Reverse joystick Y',260,210),
 check('JoystickSouthpaw','Swap joystick sticks',260,238),
 label('JoystickYawSensitivityPreLabel','Joystick yaw sensitivity',24,258),slider('JoystickYawSlider',24,282,205,28),
 label('JoystickPitchSensitivityPreLabel','Joystick pitch sensitivity',260,258),slider('JoystickPitchSlider',260,282,205,28)])
page('OptionsSubAudio.res',[
 label('SFX label','Sound effects volume',24,14),slider('SFXSlider',24,40,205),
 label('MusicVolumeLabel','Music volume',260,14),slider('MusicSlider',260,40,205),
 label('SpeakerConfigLabel','Speaker setup',24,100),combo('SpeakerSetup',24,124,205),
 label('SoundQualityLabel','Sound quality',260,100),combo('SoundQuality',260,124,205),
 label('AudioSpokenLanguageLabel','Spoken language',24,168),combo('AudioSpokenLanguage',24,192,205),
 label('CloseCaptionLabel','Captions',260,168),combo('CloseCaptionCheck',260,192,205),
 check('snd_mute_losefocus','Mute sound when the game is in the background',20,248,460),
 label('AudioHint','Changes are saved when you press Apply or OK.',24,284,450)])
page('OptionsSubVideo.res',[
 label('ResolutionLabel','Resolution',24,22),combo('Resolution',24,48,205),
 label('DisplayModeLabel','Window mode',260,22),combo('DisplayModeCombo',260,48,205),
 label('AspectRatioLabel','Aspect ratio',24,100),combo('AspectRatio',24,126,205),
 button('GammaButton','Adjust brightness...',260,126,205),button('AdvancedButton','Advanced video...',260,202,205),
 label('BorderlessHint','Borderless windowed uses the current monitor desktop size.',24,166,450),
 label('VRModeLabel','VR mode',24,214),combo('VRMode',24,240,205),
 check('HDContentButton','Use high definition content',24,278,340,kind='CheckButton',visible=0),
 ctl('Button','BenchmarkButton',0,0,0,0,visible=0),ctl('URLButton','ThirdPartyVideoCredits',0,0,0,0,visible=0)])
page('OptionsSubVoice.res',[
 check('voice_modenable','Enable voice chat',24,18,350),
 label('MicrophoneLabel','Microphone volume',30,76),ctl('Slider','#GameUI_MicrophoneVolume',30,104,205,40),
 label('ReceiveLabel','Voice receive volume',260,76),slider('VoiceReceive',260,104,205),
 ctl('ImagePanel','MicMeter',30,164,158,32,image='resource/mic_meter_dead'),
 ctl('ImagePanel','MicMeter2',30,164,158,32,image='resource/mic_meter_live'),
 button('TestMicrophone','Test microphone',260,164,205,'TestMicrophone'),check('MicBoost','Boost microphone gain',24,226,350,kind='CheckButton'),
 label('VoiceHint','Microphone controls require an available recording device.',30,272,440)])
page('OptionsSubAdvanced.res',[
 check('ContentlockButton','Content lock',28,40,160,kind='CheckButton',command='ContentControl'),
 label('ContentlockLabel','Restrict mature content with a password.',200,38,270,44,wrap=1)])
page('OptionsSubMultiplayer.res',[
 label('NameLabel','Player name',24,14),ctl('CCvarTextEntry','NameEntry',24,40,205,24,cvar_name='name'),
 label('Label1','Player model',24,86),ctl('CLabeledCommandComboBox','Player model',24,112,205,24),
 ctl('ImagePanel','ModelImage',20,151,100,90),label('ModelAvailability','Player models and model colors require multiplayer support.',24,154,205,56,wrap=1),
 label('Colors','Model colors',24,220),slider('Primary Color Slider',24,246,98,30),slider('Secondary Color Slider',130,246,98,30),
 check('High Quality Models','High quality models',20,282,215),
 label('LogoLabel','Spray image',260,14),ctl('CLabeledCommandComboBox','SpraypaintList',260,40,205,24),
 ctl('ImagePanel','LogoImage',270,80,96,96,scaleImage=1),button('ImportSprayImage','Import spray...',374,112,110,'ImportSprayImage'),
 label('DownloadFilterLabel','Allow downloads',260,192),combo('DownloadFilterCheck',260,218,205),
 label('MultiplayerHint','Crosshair and HUD colors are configured on the HUD tab.',260,256,210,48,wrap=1),
 ctl('Button','Cancel',0,0,0,0,visible=0),ctl('Button','OK',0,0,0,0,visible=0),ctl('Button','Apply',0,0,0,0,visible=0),ctl('Button','Advanced',0,0,0,0,visible=0),
 ctl('Panel','CrosshairImage',0,0,0,0,visible=0),ctl('Panel','AdvCrosshairImage',0,0,0,0,visible=0),
 ctl('ComboBox','CrosshairColorComboBox',0,0,0,0,visible=0),ctl('CLabeledCommandComboBox','CrosshairSizeComboBox',0,0,0,0,visible=0),
 ctl('CCvarToggleCheckButton','CrosshairTranslucencyCheckbox',0,0,0,0,visible=0),ctl('CCvarToggleCheckButton','LockRadarRotationCheckbox',0,0,0,0,visible=0)])
advanced=[]
fields=[('ModelDetail','Model detail'),('TextureDetail','Texture detail'),('ShaderDetail','Shader detail'),('WaterDetail','Water detail'),('ShadowDetail','Shadow detail'),('ColorCorrection','Color correction'),('AntialiasingMode','Anti-aliasing'),('FilteringMode','Texture filtering'),('VSync','Vertical sync'),('MotionBlur','Motion blur'),('HDR','High dynamic range'),('Multicore','Multicore rendering')]
for i,(name,text) in enumerate(fields):
 x=22+(i%3)*154;y=38+(i//3)*70;advanced.extend([label(name+'Label',text,x,y,146),combo(name,x,y+24,138)])
advanced.extend([label('FovLabel','Field of view',22,320,124),slider('FovSlider',154,318,180,38),label('FovValueLabel','',340,320,48),
 label('DXInfoLabel','Hardware DirectX level',22,368,220),combo('dxlabel',300,366,160),
 label('dxinstalledlabel','',22,398,200),button('AdvancedOK','OK',292,400,80,'OK'),button('AdvancedCancel','Cancel',380,400,80,'Close')])
page('OptionsSubVideoAdvancedDlg.res',advanced)
page('OptionsSubKeyboardAdvancedDlg.res',[
 check('ConsoleCheck','Enable developer console',20,38,270,kind='CheckButton'),check('FastSwitchCheck','Fast weapon switch',20,70,270,kind='CheckButton'),
 button('KeyboardAdvancedOK','OK',136,110,80,'OK'),button('KeyboardAdvancedCancel','Cancel',224,110,80,'Close')])
page('OptionsSubVideoGammaDlg.res',[
 label('GammaInfo','Adjust the slider until the dark details are visible.',20,32,340,38,wrap=1),
 label('Gamma label','Brightness',20,80,150),slider('Gamma',20,104,280,42),ctl('TextEntry','GammaEntry',312,104,55,24,NumericInputOnly=1),
 button('OKButton','OK',198,176,80),button('GammaCancel','Cancel',286,176,80,'Close')])

def quoted(value):return '"'+str(value).replace('\\','\\\\').replace('"','\\"')+'"'
OUT.mkdir(parents=True,exist_ok=True)
for file,controls in PAGES.items():
 p=OUT/file
 if p.exists():
  b=BACKUP/p.relative_to(REPO);b.parent.mkdir(parents=True,exist_ok=True)
  if not b.exists():shutil.copy2(p,b)
 lines=['// GammaSource: bound 2018 controls with the original 2003 color/font scheme.',quoted('Resource/'+file),'{' ]
 for name,values in controls.items():
  lines.extend(['\t'+quoted(name),'\t{'])
  lines.extend('\t\t'+quoted(k)+'\t'+quoted(v) for k,v in values.items())
  lines.append('\t}')
 lines.append('}')
 p.write_text('\n'.join(lines)+'\n',encoding='ascii')
 print('wrote',p)
print('Generated',len(PAGES),'compatible resources')
