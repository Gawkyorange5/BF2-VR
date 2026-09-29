using System;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Diagnostics;
using System.Collections.Generic;
using System.Web.Script.Serialization;

namespace BF2VR {
 public sealed class InstalledFile { public string Target,Backup,OriginalHash,InstalledHash; }
 public sealed class InstallState { public string Version="1.0.0-r45",Game,Profile; public List<InstalledFile> Files=new List<InstalledFile>(); public bool Complete; }
 public sealed class InstallEngine {
  public const string StateFolder=".bf2-vr";
  public static readonly string[] GameFiles={"BF2-VR.exe","RFX.dll","openxr_loader.dll","stl_8_wrapper.dll","BF2-VR-Settings.exe","vr_stereo.ini","BF2-VR-README.txt"};
  public Action<string> Log=delegate{};
  // Only used by the separate local verification executable for rollback checks.
  internal int FailAfter=-1;
  static string Full(string p){return Path.GetFullPath(p).TrimEnd(Path.DirectorySeparatorChar,Path.AltDirectorySeparatorChar);}
  static string HashFile(string p){return PeImage.Hash(File.ReadAllBytes(p));}
  static JavaScriptSerializer Json(){return new JavaScriptSerializer{MaxJsonLength=1000000};}
  static void RejectLinks(string p){for(var d=new DirectoryInfo(Directory.Exists(p)?p:Path.GetDirectoryName(p));d!=null;d=d.Parent)if(d.Exists && (d.Attributes&FileAttributes.ReparsePoint)!=0)throw new IOException("For predictable backups, choose a game/profile folder without directory links: "+p);if(File.Exists(p)&&(File.GetAttributes(p)&FileAttributes.ReparsePoint)!=0)throw new IOException("Cannot replace a linked file: "+p);}
  static void Closed(){foreach(var p in Process.GetProcesses()){using(p){string n;try{n=p.ProcessName;}catch{continue;}if(n.Equals("BF2",StringComparison.OrdinalIgnoreCase)||n.Equals("BF2-VR",StringComparison.OrdinalIgnoreCase)||n.Equals("RFX",StringComparison.OrdinalIgnoreCase)||n.Equals("BF2-VR-Settings",StringComparison.OrdinalIgnoreCase))throw new IOException("Close Battlefield 2 and the VR settings app before installing or removing BF2-VR.");}}}
  static byte[] Resource(string name){using(var s=Assembly.GetExecutingAssembly().GetManifestResourceStream(name)){if(s==null)throw new FileNotFoundException("Installer resource missing: "+name);using(var m=new MemoryStream()){s.CopyTo(m);return m.ToArray();}}}
  public static string DocumentsProfiles(){return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments),"Battlefield 2","Profiles");}
  public static string[] Profiles(){string p=DocumentsProfiles();return Directory.Exists(p)?Directory.GetDirectories(p).Where(d=>System.Text.RegularExpressions.Regex.IsMatch(Path.GetFileName(d),"^[0-9]{4}$")&&File.Exists(Path.Combine(d,"Controls.con"))).ToArray():new string[0];}
  static void Save(string dir,InstallState s){string target=Path.Combine(dir,"install.json"),tmp=target+".tmp";File.WriteAllText(tmp,Json().Serialize(s));if(File.Exists(target))File.Replace(tmp,target,null);else File.Move(tmp,target);}
  void Put(string staging,string target,string stateDir,InstallState state){
   RejectLinks(target);var f=new InstalledFile{Target=Full(target),InstalledHash=HashFile(staging)};
   if(File.Exists(target)){f.Backup=state.Files.Count.ToString("D3")+".bak";f.OriginalHash=HashFile(target);File.Copy(target,Path.Combine(stateDir,"backup",f.Backup),false);}
   state.Files.Add(f);Save(stateDir,state); // journal BEFORE changing a user file
   File.Copy(staging,target,true);
   if(HashFile(target)!=f.InstalledHash)throw new IOException("Installed file did not match: "+target);
   Log("Installed "+Path.GetFileName(target));
   if(FailAfter==state.Files.Count)throw new IOException("Verification-only simulated write failure.");
  }
  public void Install(string folder,string profile,bool shortcut,string mod){
   folder=Full(folder);Closed();PeImage.ValidateGame(folder);RejectLinks(folder);
   if(profile!=null){profile=Full(profile);
#if !INSTALLER_VERIFY
   if(!Profiles().Any(p=>String.Equals(Full(p),profile,StringComparison.OrdinalIgnoreCase)))throw new IOException("Select an existing BF2 Documents profile.");
#endif
   if(!File.Exists(Path.Combine(profile,"Controls.con")))throw new IOException("Select an existing BF2 profile after first running BF2 flat-screen.");RejectLinks(profile);}
   if(mod==null || !System.Text.RegularExpressions.Regex.IsMatch(mod,"^[a-zA-Z0-9_-]+$") || !Directory.Exists(Path.Combine(folder,"mods",mod)))throw new IOException("Choose an installed mod.");
   string stateDir=Path.Combine(folder,StateFolder);
   if(Directory.Exists(stateDir))throw new IOException("An installation or unfinished operation already exists. Use Remove / Restore first. Your previous backup will not be overwritten.");
   Directory.CreateDirectory(stateDir);Directory.CreateDirectory(Path.Combine(stateDir,"backup"));string stage=Path.Combine(stateDir,"stage");Directory.CreateDirectory(stage);
   var state=new InstallState{Game=folder,Profile=profile};Save(stateDir,state);
   try{
    Log("Checking BF2 1.5 and preparing a separate VR executable...");
    var files=new Dictionary<string,byte[]>();
    files.Add("BF2-VR.exe",new PeImage(File.ReadAllBytes(Path.Combine(folder,"BF2.exe"))).AddRfxImport());
    foreach(string name in new[]{"RFX.dll","openxr_loader.dll","stl_8_wrapper.dll","BF2-VR-Settings.exe","BF2-VR-README.txt"})files.Add(name,Resource("payload."+name));
    // Existing VR preferences are retained; fresh players get the confirmed defaults.
    if(!File.Exists(Path.Combine(folder,"vr_stereo.ini")))files.Add("vr_stereo.ini",Resource("payload.vr_stereo.ini"));
    foreach(var p in files)File.WriteAllBytes(Path.Combine(stage,p.Key),p.Value);
    Log("Building reload sounds from your own game files...");
    var audio=ReloadAudioBuilder.Generate(Path.Combine(folder,"mods","bf2","Objects_client.zip"),System.Text.Encoding.UTF8.GetString(Resource("audio-recipes.json")));
    string dll=Path.Combine(stage,"RFX.dll");string before=new PeImage(File.ReadAllBytes(dll)).TextHash();ReloadAudioBuilder.Embed(dll,audio);
    if(new PeImage(File.ReadAllBytes(dll)).TextHash()!=before)throw new IOException("Resource generation unexpectedly changed VR code.");
    foreach(var p in files)Put(Path.Combine(stage,p.Key),Path.Combine(folder,p.Key),stateDir,state);
    if(profile!=null){string con=Path.Combine(stage,"Controls.con");File.WriteAllBytes(con,Resource("payload.Controls.con"));Put(con,Path.Combine(profile,"Controls.con"),stateDir,state);}
    if(shortcut){string lnk=Path.Combine(stage,"Battlefield 2 VR.lnk");MakeShortcut(lnk,folder,mod);Put(lnk,Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory),"Battlefield 2 VR.lnk"),stateDir,state);}
    state.Complete=true;Save(stateDir,state);RemoveStage(stage);
    Log("Installed successfully. Launch Battlefield 2 VR from your desktop, or run BF2-VR.exe in the game folder.");
   }catch(Exception error){Log("Installation stopped: "+error.Message);try{Restore(stateDir,state,false);Log("Previous files restored.");}catch(Exception rollback){Log("Automatic restore needs attention: "+rollback.Message+". Keep "+stateDir+" and use Remove / Restore.");}throw;}
  }
  static void MakeShortcut(string file,string folder,string mod){
   Type type=Type.GetTypeFromProgID("WScript.Shell");if(type==null)throw new IOException("Windows shortcut service is unavailable. Install with the shortcut checkbox cleared.");
   object shell=Activator.CreateInstance(type),link=null;
   try{link=type.InvokeMember("CreateShortcut",BindingFlags.InvokeMethod,null,shell,new object[]{file});Type t=link.GetType();
    foreach(var p in new Dictionary<string,string>{{"TargetPath",Path.Combine(folder,"BF2-VR.exe")},{"WorkingDirectory",folder},{"Arguments","+menu 1 +modPath mods/"+mod},{"Description","Battlefield 2 VR — OpenXR"}})t.InvokeMember(p.Key,BindingFlags.SetProperty,null,link,new object[]{p.Value});
    t.InvokeMember("Save",BindingFlags.InvokeMethod,null,link,null);
   }finally{if(link!=null)System.Runtime.InteropServices.Marshal.FinalReleaseComObject(link);System.Runtime.InteropServices.Marshal.FinalReleaseComObject(shell);}
  }
  static void RemoveStage(string stage){if(!Directory.Exists(stage))return;RejectLinks(stage);foreach(string f in Directory.GetFiles(stage)){RejectLinks(f);File.Delete(f);}Directory.Delete(stage,false);}
  public void Uninstall(string folder){Closed();folder=Full(folder);string dir=Path.Combine(folder,StateFolder);RejectLinks(dir);var state=Json().Deserialize<InstallState>(File.ReadAllText(Path.Combine(dir,"install.json")));ValidateState(dir,state,folder);Restore(dir,state,true);}
  static void ValidateState(string dir,InstallState s,string folder){
   if(s==null || s.Game!=folder || s.Files==null || s.Files.Count>12)throw new IOException("Invalid installation journal.");
   var allowed=new HashSet<string>(GameFiles.Select(n=>Full(Path.Combine(folder,n))),StringComparer.OrdinalIgnoreCase);
   allowed.Add(Full(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory),"Battlefield 2 VR.lnk")));
   if(s.Profile!=null){string profiles=Full(DocumentsProfiles())+Path.DirectorySeparatorChar;
    // A separately compiled verification harness may use isolated profiles; production cannot.
    bool valid=Full(s.Profile).StartsWith(profiles,StringComparison.OrdinalIgnoreCase);
#if INSTALLER_VERIFY
    valid=true;
#endif
    if(!valid)throw new IOException("Journal profile is outside the BF2 Documents profile folder.");
    allowed.Add(Full(Path.Combine(s.Profile,"Controls.con")));
   }
   var seen=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
   foreach(var f in s.Files){if(!allowed.Contains(Full(f.Target)) || !seen.Add(Full(f.Target)))throw new IOException("Unsafe or duplicate path in installation journal.");RejectLinks(f.Target);
    if(f.Backup!=null){if(!System.Text.RegularExpressions.Regex.IsMatch(f.Backup,"^[0-9]{3}\\.bak$"))throw new IOException("Invalid backup path.");string b=Path.Combine(dir,"backup",f.Backup);RejectLinks(b);if(!File.Exists(b)||HashFile(b)!=f.OriginalHash)throw new IOException("Backup missing or changed; restore stopped: "+f.Target);}
   }
  }
  void Restore(string dir,InstallState s,bool protectChanges){
   ValidateState(dir,s,Full(s.Game));var conflicts=new List<string>();
   foreach(var f in s.Files.AsEnumerable().Reverse()){
    // A crash before the copy leaves the original in place: that is already restored.
    string current=File.Exists(f.Target)?HashFile(f.Target):null;
    if(f.OriginalHash!=null && current==f.OriginalHash)continue;
    if(protectChanges && current!=null && current!=f.InstalledHash){conflicts.Add(f.Target);continue;}
    if(f.Backup!=null)File.Copy(Path.Combine(dir,"backup",f.Backup),f.Target,true);else if(File.Exists(f.Target))File.Delete(f.Target);
    Log("Restored "+Path.GetFileName(f.Target));
   }
   if(conflicts.Count>0)throw new IOException("Files edited after installation were preserved, along with their backups. Restore the desired versions manually from "+dir+". Conflicts: "+String.Join(", ",conflicts));
   RemoveStage(Path.Combine(dir,"stage"));foreach(var f in s.Files)if(f.Backup!=null)File.Delete(Path.Combine(dir,"backup",f.Backup));Directory.Delete(Path.Combine(dir,"backup"),false);
   foreach(string name in new[]{"install.json","install.json.tmp"}){string p=Path.Combine(dir,name);if(File.Exists(p))File.Delete(p);}Directory.Delete(dir,false);Log("Removal complete. Original BF2.exe was never changed.");
  }
 }
}
