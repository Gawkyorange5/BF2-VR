using System;
using System.IO;
using System.Linq;
using System.Text;
using System.Reflection;
using System.Diagnostics;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Web.Script.Serialization;
using Microsoft.Win32.SafeHandles;
namespace BF2VR {
 public sealed class Dzb2CacheState {public string Source,AudioStamp,RootIniHash,RuntimeHash;}
 public static class Dzb2Launcher {
  public const string OriginalShader="4c54df27ce84d21b2924e64ff79b13e7876ce85d8e0c9c1d0abd8da73888187a";
  public static string Hash(string path){return PeImage.Hash(File.ReadAllBytes(path));}
  public static byte[] Resource(string name){using(var s=Assembly.GetExecutingAssembly().GetManifestResourceStream(name)){if(s==null)throw new IOException("Missing launcher resource: "+name);using(var m=new MemoryStream()){s.CopyTo(m);return m.ToArray();}}}
  static void Safe(string p){for(var d=new DirectoryInfo(Directory.Exists(p)?p:Path.GetDirectoryName(p));d!=null;d=d.Parent)if(d.Exists&&(d.Attributes&FileAttributes.ReparsePoint)!=0)throw new IOException("Linked runtime/cache path is not supported: "+p);if(File.Exists(p)&&(File.GetAttributes(p)&FileAttributes.ReparsePoint)!=0)throw new IOException("Linked file: "+p);}
  static void Copy(string from,string to){Safe(from);Safe(to);if(File.Exists(to)&&Hash(from)==Hash(to))return;Directory.CreateDirectory(Path.GetDirectoryName(to));string temp=to+".new";File.Copy(from,temp,true);if(File.Exists(to))File.Replace(temp,to,null);else File.Move(temp,to);}
  static void Tree(string from,string to){if(!Directory.Exists(from))return;Safe(from);Safe(to);Directory.CreateDirectory(to);foreach(var f in Directory.GetFiles(from))Copy(f,Path.Combine(to,Path.GetFileName(f)));foreach(var d in Directory.GetDirectories(from))Tree(d,Path.Combine(to,Path.GetFileName(d)));}
  [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]static extern SafeFileHandle CreateFile(string name,uint access,uint share,IntPtr security,uint creation,uint flags,IntPtr template);
  [DllImport("kernel32.dll",SetLastError=true)]static extern bool DeviceIoControl(SafeFileHandle h,uint code,byte[] input,int size,IntPtr output,int outputSize,out int returned,IntPtr overlap);
  [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]static extern uint GetFinalPathNameByHandle(SafeFileHandle h,StringBuilder path,uint length,uint flags);
  static void Junction(string link,string target){
   target=Path.GetFullPath(target).TrimEnd('\\');
   if(Directory.Exists(link)){
    using(var h=CreateFile(link,0,7,IntPtr.Zero,3,0x02000000,IntPtr.Zero)){var b=new StringBuilder(32768);if(h.IsInvalid||GetFinalPathNameByHandle(h,b,(uint)b.Capacity,0)==0||!b.ToString().Equals("\\\\?\\"+target,StringComparison.OrdinalIgnoreCase))throw new IOException("Existing mods link points somewhere else: "+link);}return;
   }
   Directory.CreateDirectory(link);byte[] sub=Encoding.Unicode.GetBytes("\\??\\"+target),print=Encoding.Unicode.GetBytes(target);var data=new byte[16+sub.Length+2+print.Length+2];
   Buffer.BlockCopy(BitConverter.GetBytes(0xA0000003u),0,data,0,4);Buffer.BlockCopy(BitConverter.GetBytes((ushort)(data.Length-8)),0,data,4,2);
   Buffer.BlockCopy(BitConverter.GetBytes((ushort)sub.Length),0,data,10,2);Buffer.BlockCopy(BitConverter.GetBytes((ushort)(sub.Length+2)),0,data,12,2);Buffer.BlockCopy(BitConverter.GetBytes((ushort)print.Length),0,data,14,2);Buffer.BlockCopy(sub,0,data,16,sub.Length);Buffer.BlockCopy(print,0,data,18+sub.Length,print.Length);
   using(var h=CreateFile(link,0x40000000,0,IntPtr.Zero,3,0x02200000,IntPtr.Zero)){int n;if(h.IsInvalid||!DeviceIoControl(h,0x900a4,data,data.Length,IntPtr.Zero,0,out n,IntPtr.Zero))throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error(),"Could not create the local mods junction.");}
  }
  public static string CacheRoot(string game){return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"BF2-VR","DZB2",PeImage.Hash(Encoding.UTF8.GetBytes(Path.GetFullPath(game).TrimEnd('\\').ToLowerInvariant())).Substring(0,16));}
  public static string Prepare(string game,string cache,Action<string> log){
   game=Path.GetFullPath(game).TrimEnd('\\');cache=Path.GetFullPath(cache);Safe(game);Safe(cache);PeImage.ValidateGame(game);
   if(!File.Exists(Path.Combine(game,"mods","DZB2","Mod.desc")))throw new IOException("Install the original DZB2 mod into this game's mods\\DZB2 folder, play a map flat-screen first, then run this command again.");
   foreach(var n in new[]{"RFX.dll","BF2-VR.exe","BF2-VR-Settings.exe","vr_stereo.ini"})if(!File.Exists(Path.Combine(game,n)))throw new IOException("Run BF2-VR Setup in this game folder first. Missing: "+n);
   if(File.Exists(Path.Combine(game,"d3d9.dll")))throw new IOException("A local d3d9.dll renderer is installed. This DZB2 compatibility launcher was tested with native Direct3D 9. Restore your normal renderer before using it; no files changed.");
   if(!File.Exists(Path.Combine(game,"d3dx9_25.dll"))||Hash(Path.Combine(game,"d3dx9_25.dll"))!=OriginalShader)throw new IOException("DZB2 needs the supported original d3dx9_25.dll from your working game. Setup does not distribute or replace Microsoft's DLL.");
   if(new PeImage(File.ReadAllBytes(Path.Combine(game,"RFX.dll"))).TextHash()!=new PeImage(Resource("payload.RFX.dll")).TextHash())throw new IOException("The installed VR code is newer or different from this launcher. Use the launcher supplied with that build to preserve your installed custom build.");
   Directory.CreateDirectory(cache);string stateFile=Path.Combine(cache,"cache.json");Safe(stateFile);var json=new JavaScriptSerializer();var state=File.Exists(stateFile)?json.Deserialize<Dzb2CacheState>(File.ReadAllText(stateFile)):new Dzb2CacheState{Source=game};
   if(state.Source!=game)throw new IOException("Cache belongs to another game installation.");
   string dest=Path.Combine(cache,"Game");Safe(dest);Directory.CreateDirectory(dest);log("Preparing DZB2 compatibility files in "+dest);
   var special=new HashSet<string>(new[]{"bf2.exe","bf2-vr.exe","bf2-vr-settings.exe","pylib-2.3.4.zip","00000000.256","a2mtest.dds"},StringComparer.OrdinalIgnoreCase);
   var suffix=new HashSet<string>(new[]{".dll",".con",".cfg",".ini"},StringComparer.OrdinalIgnoreCase);
   foreach(var f in Directory.GetFiles(game)){
    string n=Path.GetFileName(f);if(n.Equals("RFX.dll",StringComparison.OrdinalIgnoreCase)||n.Equals("vr_stereo.ini",StringComparison.OrdinalIgnoreCase)||n.Equals("dinput8.dll",StringComparison.OrdinalIgnoreCase)||n.EndsWith("shader-compat.dll",StringComparison.OrdinalIgnoreCase))continue;
    if(special.Contains(n)||suffix.Contains(Path.GetExtension(n))){if(n.Equals("d3dx9_25.dll",StringComparison.OrdinalIgnoreCase))n="bf2vr_d3dx25_original.dll";Copy(f,Path.Combine(dest,n));}
   }
   foreach(string name in new[]{"python","Settings","Profiles"})Tree(Path.Combine(game,name),Path.Combine(dest,name));
   Junction(Path.Combine(dest,"mods"),Path.Combine(game,"mods"));
   string shader=Path.Combine(dest,"d3dx9_25.dll");Safe(shader);File.WriteAllBytes(shader,Resource("payload.DZB2-shader-compat.dll"));
   string rootIni=Path.Combine(game,"vr_stereo.ini"),localIni=Path.Combine(dest,"vr_stereo.ini");string iniHash=Hash(rootIni);
   if(!File.Exists(localIni)||state.RootIniHash!=iniHash){if(File.Exists(localIni)){Safe(localIni+".before-root-settings.bak");File.Copy(localIni,localIni+".before-root-settings.bak",true);}Copy(rootIni,localIni);}state.RootIniHash=iniHash;
   string recipes=Encoding.UTF8.GetString(Resource("audio-recipes.json"));var stamp=new StringBuilder(Hash(Path.Combine(game,"RFX.dll")));
   foreach(var path in ReloadAudioBuilder.Recipes(recipes).Where(a=>a.archive!="").Select(a=>a.archive).Distinct()){
    var f=new FileInfo(Path.Combine(game,path.Replace('/',Path.DirectorySeparatorChar)));stamp.Append(path).Append(f.Exists?f.Length:0).Append(f.Exists?f.LastWriteTimeUtc.Ticks:0);
   }
   string audioStamp=PeImage.Hash(Encoding.UTF8.GetBytes(stamp.ToString()+PeImage.Hash(Encoding.UTF8.GetBytes(recipes))));
   string runtime=Path.Combine(dest,"RFX.dll");Safe(runtime);
   if(!File.Exists(runtime)||state.AudioStamp!=audioStamp||state.RuntimeHash!=Hash(runtime)){
    string temp=runtime+".new";Safe(temp);File.Copy(Path.Combine(game,"RFX.dll"),temp,true);
    ReloadAudioBuilder.Embed(temp,ReloadAudioBuilder.Generate(game,recipes,log));
    if(new PeImage(File.ReadAllBytes(temp)).TextHash()!=new PeImage(File.ReadAllBytes(Path.Combine(game,"RFX.dll"))).TextHash())throw new IOException("Generated audio changed VR code.");
    if(File.Exists(runtime))File.Replace(temp,runtime,null);else File.Move(temp,runtime);state.AudioStamp=audioStamp;state.RuntimeHash=Hash(runtime);
   }
   string stateTemp=stateFile+".tmp";Safe(stateTemp);File.WriteAllText(stateTemp,json.Serialize(state));if(File.Exists(stateFile))File.Replace(stateTemp,stateFile,null);else File.Move(stateTemp,stateFile);
   return dest;
  }
 }
#if DZB2_LAUNCHER
 static class LauncherProgram {
  [STAThread] static int Main(string[] args){
   try{string game=AppDomain.CurrentDomain.BaseDirectory;using(var maintenance=new MaintenanceGate(game)){string cache=Dzb2Launcher.CacheRoot(game);
    using(var mutex=new System.Threading.Mutex(false,"Local\\BF2VR-DZB2-"+Path.GetFileName(cache))){if(!mutex.WaitOne(0)){Console.WriteLine("DZB2 VR is already preparing or running.");return 1;}
     try{foreach(var p in Process.GetProcesses()){using(p){string n=p.ProcessName;if(n.Equals("BF2",StringComparison.OrdinalIgnoreCase)||n.Equals("BF2-VR",StringComparison.OrdinalIgnoreCase)||n.Equals("RFX",StringComparison.OrdinalIgnoreCase)||n.Equals("BF2-VR-Settings",StringComparison.OrdinalIgnoreCase))throw new IOException("Close Battlefield 2 and VR Settings before launching DZB2.");}}
      string folder=Dzb2Launcher.Prepare(game,cache,Console.WriteLine);Console.WriteLine("Use Medium Lighting in DZB2. Connect the headset before launch.");
      var start=new ProcessStartInfo(Path.Combine(folder,"BF2-VR.exe"),"+modPath mods/DZB2 +menu 1 +fullscreen 0 +szx 1280 +szy 720 +restart"){UseShellExecute=false,WorkingDirectory=folder};start.EnvironmentVariables["BF2VR_SHADER_GUARD"]="1";
      using(var p=Process.Start(start)){p.WaitForExit();Console.WriteLine("VR log and saved DZB2 preferences: "+folder);return p.ExitCode;}
     }finally{mutex.ReleaseMutex();}
    }}
   }catch(Exception e){Console.WriteLine("DZB2 VR stopped: "+e.Message);Console.WriteLine("No original game executable or mod assets were replaced.");return 1;}
  }
 }
#endif
}
