"""Build the single-file public installer using Windows' .NET Framework compiler.
No game files, network calls, installed-game changes, or headset needed.
"""
from pathlib import Path
import subprocess, os
root=Path(__file__).resolve().parents[1]
out=root/'build';out.mkdir(exist_ok=True)
csc=Path(os.environ.get('WINDIR',r'C:\Windows'))/'Microsoft.NET/Framework/v4.0.30319/csc.exe'
args=[str(csc),'/nologo','/platform:x86','/optimize+','/target:winexe','/out:'+str(out/'BF2-VR-Setup.exe')]
args += ['/r:'+n+'.dll' for n in ['System.Windows.Forms','System.Drawing','System.Web.Extensions','System.IO.Compression','System.IO.Compression.FileSystem']]
for name in ['RFX.dll','openxr_loader.dll','stl_8_wrapper.dll','BF2-VR-Settings.exe','BF2-VR-README.txt']:
 args.append('/resource:'+str(root/'payload'/name)+',payload.'+name)
for file,name in [('profiles/Controls.con','payload.Controls.con'),('config/vr_stereo.ini','payload.vr_stereo.ini'),('installer/audio-recipes.json','audio-recipes.json')]:
 args.append('/resource:'+str(root/file)+','+name)
args += [str(root/'installer'/n) for n in ['PeImage.cs','ReloadAudioBuilder.cs','InstallEngine.cs','SetupForm.cs']]
subprocess.run(args,check=True)
print(out/'BF2-VR-Setup.exe')
