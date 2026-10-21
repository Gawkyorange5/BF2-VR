using System;using System.IO;using System.Linq;using System.Text;using System.Runtime.InteropServices;using System.Collections.Generic;using System.Web.Script.Serialization;using BF2VR;
class InstallerChecks {
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]static extern bool CreateHardLink(string newName,string existing,IntPtr security);
 static void Link(string from,string to){Directory.CreateDirectory(Path.GetDirectoryName(to));if(!CreateHardLink(to,from,IntPtr.Zero))throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());}
 static void Check(bool ok,string label){if(!ok)throw new Exception("FAIL: "+label);Console.WriteLine("PASS: "+label);}
 static string H(string p){return PeImage.Hash(File.ReadAllBytes(p));}
 static string Text(string p){return new PeImage(File.ReadAllBytes(p)).TextHash();}
 static bool Refused(Action action){try{action();return false;}catch(IOException){return true;}catch(InvalidDataException){return true;}}
 static int Main(string[] args){try{
  string game=args[0],root=Path.GetFullPath(args[1]),repo=Path.GetFullPath(args[2]),old=args[3],baseline=args[4];
  if(Directory.Exists(root))throw new Exception("Use a new fixture directory.");Directory.CreateDirectory(root);
  string f=Path.Combine(root,"Game with spaces"),profile=Path.Combine(root,"test profile");Directory.CreateDirectory(f);Directory.CreateDirectory(profile);
  foreach(string n in new[]{"BF2.exe","RendDX9.dll","d3dx9_25.dll"})File.Copy(Path.Combine(game,n),Path.Combine(f,n));
  string recipes=File.ReadAllText(Path.Combine(repo,"installer","audio-recipes.json"));var audio=ReloadAudioBuilder.Recipes(recipes);
  foreach(string a in audio.Where(a=>a.mod=="bf2"&&a.archive!="").Select(a=>a.archive).Distinct())Link(Path.Combine(game,a.Replace('/','\\')),Path.Combine(f,a.Replace('/','\\')));
  File.WriteAllText(Path.Combine(f,"mods","bf2","init.con"),"rem fixture\r\n");File.WriteAllText(Path.Combine(profile,"Controls.con"),"original-controls");
  string stock=H(Path.Combine(f,"BF2.exe")),renderer=H(Path.Combine(f,"RendDX9.dll"));PeImage.ValidateGame(f);
  var bytes=File.ReadAllBytes(Path.Combine(f,"BF2.exe"));var pe=new PeImage(bytes);byte[] patched=pe.AddRfxImport();Check(new PeImage(patched).HasRfx()&&new PeImage(patched).TextHash()==pe.TextHash(),"EXE import edit preserves executable code");Check(PeImage.Hash(new PeImage(patched).AddRfxImport())==PeImage.Hash(patched),"EXE import operation is idempotent");
  var e=new InstallEngine{Log=Console.WriteLine,FailAfter=3};Check(Refused(()=>e.Install(f,profile,false,"bf2")),"injected installation failure reported");Check(!File.Exists(Path.Combine(f,"RFX.dll"))&&!Directory.Exists(Path.Combine(f,".bf2-vr")),"failed fresh install rolls back completely");
  e.FailAfter=-1;e.Install(f,profile,false,"bf2",0);Check(Text(Path.Combine(f,"RFX.dll"))==Text(baseline),"clean install keeps exact accepted r134 code");
  Check(File.ReadAllText(Path.Combine(f,"vr_stereo.ini")).Contains("resolution_mode=0"),"headset resolution choice saved");
  Check(H(Path.Combine(profile,"Controls.con"))==H(Path.Combine(repo,"profiles","Controls.con")),"current controller profile installed");
  Check(File.Exists(Path.Combine(f,"Launch DZB2 VR.cmd"))&&File.Exists(Path.Combine(f,"BF2-VR-DZB2.exe")),"DZB2 command/helper installed before DZB2 is present");
  byte[] exe=File.ReadAllBytes(Path.Combine(f,"BF2-VR.exe"));int nt=BitConverter.ToInt32(exe,0x3c);Check((BitConverter.ToUInt16(exe,nt+22)&32)!=0,"generated VR executable is large-address-aware");
  File.Copy(Path.Combine(f,"RFX.dll"),Path.Combine(root,"stock-generated.dll"));e.Uninstall(f);Check(!File.Exists(Path.Combine(f,"RFX.dll"))&&File.ReadAllText(Path.Combine(profile,"Controls.con"))=="original-controls","fresh uninstall restores profile and removes owned runtime");
  // Emulate an existing v1 installation subsequently updated to accepted r134.
  Directory.CreateDirectory(Path.Combine(f,".bf2-vr","backup"));File.Copy(baseline,Path.Combine(f,"RFX.dll"));
  var oldState=new InstallState{Version="1.0.0-r45",Game=f,Complete=true};oldState.Files.Add(new InstalledFile{Target=Path.Combine(f,"RFX.dll"),InstalledHash=H(old)});
  string oldJson=new JavaScriptSerializer().Serialize(oldState);File.WriteAllText(Path.Combine(f,".bf2-vr","install.json"),oldJson);
  string prefs="[graphics]\r\nresolution_mode=0\r\n[body]\r\ncustom_slot=1.234\r\n";File.WriteAllText(Path.Combine(f,"vr_stereo.ini"),prefs);
  e.FailAfter=3;Check(Refused(()=>e.Install(f,null,false,"bf2")),"failed upgrade reported");Check(H(Path.Combine(f,"RFX.dll"))==H(baseline)&&File.ReadAllText(Path.Combine(f,".bf2-vr","install.json"))==oldJson,"failed upgrade restores r134 and exact old journal");
  e.FailAfter=-1;e.Install(f,null,false,"bf2");Check(File.ReadAllText(Path.Combine(f,"vr_stereo.ini"))==prefs,"Keep current preserves settings and slot data byte for byte");
  e.Uninstall(f);Check(H(Path.Combine(f,"RFX.dll"))==H(baseline)&&File.ReadAllText(Path.Combine(f,".bf2-vr","install.json"))==oldJson,"successful upgrade can return to exact r134 with original journal");
  e.Install(f,null,false,"bf2",2);Check(File.ReadAllText(Path.Combine(f,"vr_stereo.ini")).Contains("resolution_mode=2")&&File.ReadAllText(Path.Combine(f,"vr_stereo.ini")).Contains("custom_slot=1.234"),"game resolution changes only requested preference");e.Uninstall(f);Check(File.ReadAllText(Path.Combine(f,"vr_stereo.ini"))==prefs,"restore recovers exact pre-choice settings");
  File.Copy(old,Path.Combine(f,"RFX.dll"),true);e.Install(f,null,false,"bf2");e.Uninstall(f);Check(H(Path.Combine(f,"RFX.dll"))==H(old),"public v1 upgrade and restore");
  File.WriteAllBytes(Path.Combine(f,"RFX.dll"),new byte[9]);Check(Refused(()=>e.Install(f,null,false,"bf2")),"unknown/newer runtime rejected before changes");File.Copy(baseline,Path.Combine(f,"RFX.dll"),true);
  string otherLoader=Path.Combine(f,"dinput8.dll");File.WriteAllText(otherLoader,"another mod's loader");Check(Refused(()=>e.Install(f,null,false,"bf2",-1,true))&&File.ReadAllText(otherLoader)=="another mod's loader","another mod's optional loader is protected");File.Delete(otherLoader);
  // Install DZB2 after the base installation. Archive links are read-only inputs.
  foreach(string a in audio.Where(a=>a.mod=="DZB2"&&a.archive!="").Select(a=>a.archive).Distinct())Link(Path.Combine(game,a.Replace('/','\\')),Path.Combine(f,a.Replace('/','\\')));
  File.Copy(Path.Combine(game,"mods","DZB2","Mod.desc"),Path.Combine(f,"mods","DZB2","Mod.desc"));
  e.Install(f,null,false,"bf2",-1,true);Check(File.Exists(Path.Combine(f,"dinput8.dll")),"optional original-executable loader installed");
  string cache=Path.Combine(root,"DZB2 cache");string dest=Dzb2Launcher.Prepare(f,cache,Console.WriteLine);
  Check(File.Exists(Path.Combine(dest,"mods","DZB2","Mod.desc")),"DZB2 mods junction resolves to installed assets");Check(H(Path.Combine(dest,"d3dx9_25.dll"))==PeImage.Hash(Dzb2Launcher.Resource("payload.DZB2-shader-compat.dll")),"exact tested shader proxy installed only in compatibility copy");
  Check(H(Path.Combine(f,"d3dx9_25.dll"))==Dzb2Launcher.OriginalShader,"original root renderer dependency unchanged");Check(!File.Exists(Path.Combine(dest,"dinput8.dll")),"renamed compatibility runtime does not copy optional Steam loader");
  Check(Text(Path.Combine(dest,"RFX.dll"))==Text(baseline),"DZB2 runtime preserves exact r134 code");File.Copy(Path.Combine(dest,"RFX.dll"),Path.Combine(root,"dzb2-generated.dll"));
  File.AppendAllText(Path.Combine(dest,"vr_stereo.ini"),"\r\n[local]\r\nretained=1\r\n");string saved=H(Path.Combine(dest,"vr_stereo.ini"));Dzb2Launcher.Prepare(f,cache,Console.WriteLine);Check(H(Path.Combine(dest,"vr_stereo.ini"))==saved,"DZB2 preferences survive subsequent launches");
  byte[] installedIni=File.ReadAllBytes(Path.Combine(f,"vr_stereo.ini"));File.AppendAllText(Path.Combine(f,"vr_stereo.ini"),"\r\n[new-root]\r\nchanged=1\r\n");Dzb2Launcher.Prepare(f,cache,Console.WriteLine);Check(H(Path.Combine(dest,"vr_stereo.ini"))==H(Path.Combine(f,"vr_stereo.ini"))&&File.Exists(Path.Combine(dest,"vr_stereo.ini.before-root-settings.bak")),"changed main settings propagated with compatibility backup");File.WriteAllBytes(Path.Combine(f,"vr_stereo.ini"),installedIni);
  e.Uninstall(f);Check(!File.Exists(Path.Combine(f,"dinput8.dll")),"restore removes owned Steam loader");
  e.Install(f,profile,false,"bf2");string control=File.ReadAllText(Path.Combine(profile,"Controls.con"));File.WriteAllText(Path.Combine(profile,"Controls.con"),"user-edit");Check(Refused(()=>e.Uninstall(f))&&File.ReadAllText(Path.Combine(profile,"Controls.con"))=="user-edit","later user edits protected on restore");File.WriteAllText(Path.Combine(profile,"Controls.con"),control);e.Uninstall(f);
  Check(H(Path.Combine(f,"BF2.exe"))==stock&&H(Path.Combine(f,"RendDX9.dll"))==renderer,"original game executable and renderer unchanged through all checks");
  Console.WriteLine("Completed isolated verification; no game process launched.");return 0;
 }catch(Exception e){Console.Error.WriteLine(e);return 1;}}
}
