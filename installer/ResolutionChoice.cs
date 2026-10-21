using System;
using System.ComponentModel;
using System.Globalization;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

namespace BF2VR {
 // Shared by Setup and updater. Runtime updates and preference changes are
 // separate operations: changing resolution never reinstalls the DLL.
 internal static class ResolutionChoice {
  public static readonly string[] Labels={
   "Keep current resolution setting",
   "Headset recommended - automatic resolution",
   "Game resolution - original BF2 rendering"
  };
  public static int Mode(int index){if(index<0||index>2)throw new ArgumentOutOfRangeException("index");return index==0?-1:index==1?0:2;}
  public static string Describe(int index){
   if(index==1)return "Uses the connected OpenXR runtime's recommended size per eye. More pixels can cost FPS.";
   if(index==2)return "Uses BF2's configured render size. Can improve FPS when that size is lower. Restart to change.";
   return "Preserves your current choice, including Custom. Change later in VR Settings > Graphics; restart BF2.";
  }
  [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]
  static extern bool WritePrivateProfileString(string section,string key,string value,string path);
  [DllImport("kernel32.dll",CharSet=CharSet.Unicode)]
  static extern uint GetPrivateProfileString(string section,string key,string fallback,StringBuilder value,uint size,string path);
  public static void Validate(int mode){if(mode!=-1&&mode!=0&&mode!=2)throw new ArgumentOutOfRangeException("mode");}
  public static void EnableSteam(string stagedIni){if(!WritePrivateProfileString("graphics","steam_launch","1",Path.GetFullPath(stagedIni)))throw new Win32Exception(Marshal.GetLastWin32Error());}
  public static void Apply(string stagedIni,int mode){
   Validate(mode);if(mode==-1)return;
   if(!File.Exists(stagedIni))throw new IOException("Resolution settings were not staged.");
   string path=Path.GetFullPath(stagedIni),value=mode.ToString(CultureInfo.InvariantCulture);
   if(!WritePrivateProfileString("graphics","resolution_mode",value,path))throw new Win32Exception(Marshal.GetLastWin32Error(),"Could not save the resolution choice.");
   var result=new StringBuilder(32);GetPrivateProfileString("graphics","resolution_mode","",result,32,path);
   if(result.ToString()!=value)throw new IOException("The staged resolution choice could not be confirmed.");
  }
  static void Safe(string path){
   for(string p=Path.GetFullPath(path);!String.IsNullOrEmpty(p);p=Path.GetDirectoryName(p))
    if((File.Exists(p)||Directory.Exists(p))&&(File.GetAttributes(p)&FileAttributes.ReparsePoint)!=0)
     throw new IOException("Select a game folder without linked files or directories: "+p);
  }
  public static void Save(string folder,int mode,byte[] defaults,Action<string> log){
   Validate(mode);
   if(mode==-1){log("Kept the current resolution setting. No files changed.");return;}
   string ini=Path.Combine(Path.GetFullPath(folder),"vr_stereo.ini");Safe(ini);
   bool existed=File.Exists(ini);byte[] original=existed?File.ReadAllBytes(ini):null;
   var current=new StringBuilder(32);GetPrivateProfileString("graphics","resolution_mode","",current,32,ini);
   if(existed&&current.ToString()==mode.ToString(CultureInfo.InvariantCulture)){
    log("That resolution mode is already selected. No files changed. Start BF2 normally.");return;
   }
   string suffix=DateTime.UtcNow.ToString("yyyyMMdd-HHmmss",CultureInfo.InvariantCulture)+"-"+Guid.NewGuid().ToString("N");
   string stage=ini+"."+suffix+".tmp",backup=ini+".before-resolution-"+suffix+".bak";
   try{
    using(var stream=new FileStream(stage,FileMode.CreateNew,FileAccess.Write,FileShare.None)){
     byte[] bytes=existed?original:defaults;stream.Write(bytes,0,bytes.Length);stream.Flush(true);
    }
    Apply(stage,mode);
    // Refuse to overwrite an edit made by another program while staging.
    if(existed){
     if(!File.Exists(ini)||PeImage.Hash(File.ReadAllBytes(ini))!=PeImage.Hash(original))
      throw new IOException("VR settings changed in another program. Close that program, then save again.");
     File.Replace(stage,ini,backup);
    }else File.Move(stage,ini);
    log(mode==0?"Saved: Headset recommended resolution.":"Saved: BF2 game resolution.");
    if(existed)log("Previous settings saved as "+Path.GetFileName(backup));
    log("Start BF2 normally. Resolution takes effect at the next launch.");
   }finally{if(File.Exists(stage))File.Delete(stage);}
  }
 }
}
