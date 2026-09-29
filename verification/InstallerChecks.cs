using System;
using System.IO;
using System.Linq;
using BF2VR;
class InstallerChecks {
 static void Check(bool ok,string label){if(!ok)throw new Exception("FAIL: "+label);Console.WriteLine("PASS: "+label);}
 static int Main(string[] args){try{
  string folder=Path.GetFullPath(args[0]),stock=args[1],game=args[2],repo=args[3],baseline=args[4];
  if(Directory.Exists(folder))throw new Exception("Use a new isolated directory for verification.");
  Directory.CreateDirectory(folder);Directory.CreateDirectory(Path.Combine(folder,"mods","bf2"));
  File.Copy(stock,Path.Combine(folder,"BF2.exe"));File.Copy(Path.Combine(game,"RendDX9.dll"),Path.Combine(folder,"RendDX9.dll"));
  File.Copy(Path.Combine(game,"mods","bf2","Objects_client.zip"),Path.Combine(folder,"mods","bf2","Objects_client.zip"));
  string profile=Path.Combine(folder,"test-profile");Directory.CreateDirectory(profile);File.WriteAllText(Path.Combine(profile,"Controls.con"),"original-profile-for-restoration");
  File.WriteAllText(Path.Combine(folder,"RFX.dll"),"previous-mod-for-backup");File.WriteAllText(Path.Combine(folder,"vr_stereo.ini"),"[existing]\nretained=1\n");
  string original=PeImage.Hash(File.ReadAllBytes(stock));var pe=new PeImage(File.ReadAllBytes(stock));
  Check(!pe.HasRfx(),"stock executable has no RFX import");PeImage.ValidateGame(folder);
  byte[] patched=pe.AddRfxImport();Check(new PeImage(patched).HasRfx(),"RFX import added to a copy");Check(new PeImage(patched).TextHash()==pe.TextHash(),"game executable code preserved");Check(PeImage.Hash(new PeImage(patched).AddRfxImport())==PeImage.Hash(patched),"import patch is idempotent");
  bool refused=false;byte[] bad=(byte[])File.ReadAllBytes(stock).Clone();bad[1024]^=1;File.WriteAllBytes(Path.Combine(folder,"BF2.exe"),bad);try{PeImage.ValidateGame(folder);}catch(InvalidDataException){refused=true;}Check(refused,"unsupported game code rejected");File.Copy(stock,Path.Combine(folder,"BF2.exe"),true);
  refused=false;try{new PeImage(new byte[20]);}catch(InvalidDataException){refused=true;}Check(refused,"truncated executable rejected");
  var cues=ReloadAudioBuilder.Generate(Path.Combine(folder,"mods","bf2","Objects_client.zip"),File.ReadAllText(Path.Combine(repo,"installer","audio-recipes.json")));
  var recipes=new System.Web.Script.Serialization.JavaScriptSerializer().Deserialize<AudioRecipe[]>(File.ReadAllText(Path.Combine(repo,"installer","audio-recipes.json")));
  int exact=0;foreach(var a in recipes){byte[] expected=File.ReadAllBytes(Path.Combine(baseline,"reload_audio",a.weapon+"-"+((a.id-1100)%4)+".pcm"));var actual=cues[a.id];Check(actual.Length==expected.Length,"audio cue length "+a.id);int max=0;for(int i=0;i<actual.Length;i+=2)max=Math.Max(max,Math.Abs(BitConverter.ToInt16(actual,i)-BitConverter.ToInt16(expected,i)));if(max==0)exact++;Check(max<=1,"audio cue sample parity "+a.id);}
  Console.WriteLine("AUDIO: "+exact+"/116 bit-identical; all within 1 PCM16 quantization unit.");
  string shortcut=Path.Combine(folder,"probe.lnk");typeof(InstallEngine).GetMethod("MakeShortcut",System.Reflection.BindingFlags.NonPublic|System.Reflection.BindingFlags.Static).Invoke(null,new object[]{shortcut,folder,"bf2"});Check(File.Exists(shortcut),"desktop shortcut creation into isolated folder");
  Type shellType=Type.GetTypeFromProgID("WScript.Shell");object shell=Activator.CreateInstance(shellType);object link=shellType.InvokeMember("CreateShortcut",System.Reflection.BindingFlags.InvokeMethod,null,shell,new object[]{shortcut});var lt=link.GetType();string targetPath=(string)lt.InvokeMember("TargetPath",System.Reflection.BindingFlags.GetProperty,null,link,null);string wd=(string)lt.InvokeMember("WorkingDirectory",System.Reflection.BindingFlags.GetProperty,null,link,null);string arguments=(string)lt.InvokeMember("Arguments",System.Reflection.BindingFlags.GetProperty,null,link,null);Check(String.Equals(targetPath,Path.Combine(folder,"BF2-VR.exe"),StringComparison.OrdinalIgnoreCase)&&String.Equals(wd,folder,StringComparison.OrdinalIgnoreCase)&&arguments=="+menu 1 +modPath mods/bf2","shortcut target, game directory and mod arguments");System.Runtime.InteropServices.Marshal.FinalReleaseComObject(link);System.Runtime.InteropServices.Marshal.FinalReleaseComObject(shell);
  var engine=new InstallEngine{Log=Console.WriteLine,FailAfter=3};bool failed=false;try{engine.Install(folder,profile,false,"bf2");}catch(IOException){failed=true;}
  Check(failed&&!Directory.Exists(Path.Combine(folder,".bf2-vr")),"simulated mid-install failure fully rolled back");Check(File.ReadAllText(Path.Combine(folder,"RFX.dll"))=="previous-mod-for-backup","prior DLL restored after failure");Check(!File.Exists(Path.Combine(folder,"BF2-VR.exe")),"generated executable removed after failure");
  engine.FailAfter=-1;engine.Install(folder,profile,false,"bf2");
  Check(PeImage.Hash(File.ReadAllBytes(Path.Combine(folder,"BF2.exe")))==original,"original BF2.exe untouched");Check(File.ReadAllText(Path.Combine(folder,"vr_stereo.ini"))=="[existing]\nretained=1\n","existing INI preserved");Check(PeImage.Hash(File.ReadAllBytes(Path.Combine(profile,"Controls.con")))==PeImage.Hash(File.ReadAllBytes(Path.Combine(repo,"profiles","Controls.con"))),"recommended bindings installed");
  refused=false;try{engine.Install(folder,profile,false,"bf2");}catch(IOException){refused=true;}Check(refused,"existing backup journal cannot be overwritten");
  // Keep an installed DLL outside the fixture for the native loader smoke check.
  File.Copy(Path.Combine(folder,"RFX.dll"),Path.Combine(Path.GetDirectoryName(folder),"RFX-generated.dll"),true);
  engine.Uninstall(folder);Check(File.ReadAllText(Path.Combine(profile,"Controls.con"))=="original-profile-for-restoration","profile restored");Check(File.ReadAllText(Path.Combine(folder,"RFX.dll"))=="previous-mod-for-backup","previous mod restored");Check(!File.Exists(Path.Combine(folder,"BF2-VR.exe"))&&!Directory.Exists(Path.Combine(folder,".bf2-vr")),"uninstall removes owned files and journal");
  File.Delete(Path.Combine(folder,"vr_stereo.ini"));engine.Install(folder,profile,false,"bf2");Check(File.Exists(Path.Combine(folder,"vr_stereo.ini")),"fresh settings installed");
  string target=Path.Combine(profile,"Controls.con"),installed=File.ReadAllText(target);File.WriteAllText(target,"user-edited-after-install");refused=false;try{engine.Uninstall(folder);}catch(IOException){refused=true;}Check(refused && File.ReadAllText(target)=="user-edited-after-install" && Directory.Exists(Path.Combine(folder,".bf2-vr")),"post-install edits and backups preserved on conflict");
  File.WriteAllText(target,installed);engine.Uninstall(folder);Check(File.ReadAllText(target)=="original-profile-for-restoration","conflict recovery restores original bindings");
  Check(PeImage.Hash(File.ReadAllBytes(Path.Combine(folder,"BF2.exe")))==original,"original executable unchanged after all checks");
  return 0;
 }catch(Exception e){Console.Error.WriteLine(e);return 1;}}
}
