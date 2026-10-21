using System;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Security.Cryptography;
using System.Windows.Forms;
using System.Collections.Generic;
class PackageChecks {
 static void Check(bool ok,string label){if(!ok)throw new Exception(label);Console.WriteLine("PASS: "+label);}
 static byte[] Resource(Assembly a,string name){using(var s=a.GetManifestResourceStream(name)){if(s==null)throw new Exception("Missing "+name);using(var b=new MemoryStream()){s.CopyTo(b);return b.ToArray();}}}
 static void Match(Assembly a,string resource,string file){Check(Resource(a,resource).SequenceEqual(File.ReadAllBytes(file)),"Final executable embeds exact "+resource);}
 [STAThread]static int Main(string[] args){try{
  string root=Path.GetFullPath(args[0]),fixture=Path.GetFullPath(args[1]);var a=Assembly.LoadFile(Path.Combine(root,"build","BF2-VR-Setup.exe"));
  var names=new[]{"RFX.dll","openxr_loader.dll","stl_8_wrapper.dll","BF2-VR-Settings.exe","BF2-VR-README.txt","BF2-VR-DZB2.exe","Launch DZB2 VR.cmd","dinput8.dll"};
  foreach(string n in names)Match(a,"payload."+n,Path.Combine(root,"payload",n));
  Match(a,"payload.Controls.con",Path.Combine(root,"profiles","Controls.con"));Match(a,"payload.vr_stereo.ini",Path.Combine(root,"config","vr_stereo.ini"));Match(a,"audio-recipes.json",Path.Combine(root,"installer","audio-recipes.json"));
  Check(a.GetManifestResourceNames().Length==11,"Setup resource allowlist has no additional bundled files");
  var helper=Assembly.Load(Resource(a,"payload.BF2-VR-DZB2.exe"));
  foreach(string n in new[]{"RFX.dll","DZB2-shader-compat.dll"})Match(helper,"payload."+n,Path.Combine(root,"payload",n));
  Match(helper,"audio-recipes.json",Path.Combine(root,"installer","audio-recipes.json"));Check(helper.GetManifestResourceNames().Length==3,"DZB2 helper resource allowlist");
  Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
  using(var f=(Form)Activator.CreateInstance(a.GetType("BF2VR.SetupForm"))){
   var flags=BindingFlags.Instance|BindingFlags.NonPublic;
   var resolution=(ComboBox)f.GetType().GetField("resolution",flags).GetValue(f);
   Check(resolution.Items.Count==3&&resolution.SelectedIndex==0,"Setup defaults to Keep current and offers both render modes");
   Check(!((CheckBox)f.GetType().GetField("steam",flags).GetValue(f)).Checked,"Steam loader is optional and off by default");
   Check(!((CheckBox)f.GetType().GetField("ready",flags).GetValue(f)).Text.ToLowerInvariant().Contains("hotfix"),"Readiness checkbox does not require everybody to install hotfix");
  }
  var pe=a.GetType("BF2VR.PeImage");var validate=pe.GetMethod("ValidateGame",BindingFlags.Public|BindingFlags.Static);
  string g=Path.Combine(fixture,"Game with spaces");validate.Invoke(null,new object[]{g});Console.WriteLine("PASS: compatible existing installation accepted without downloading a hotfix");
  string older=args[2];string temp=Path.Combine(fixture,"earlier renderer check");Directory.CreateDirectory(temp);
  File.Copy(Path.Combine(g,"BF2.exe"),Path.Combine(temp,"BF2.exe"));File.Copy(older,Path.Combine(temp,"RendDX9.dll"));
  bool rejected=false;try{validate.Invoke(null,new object[]{temp});}catch(TargetInvocationException e){rejected=e.InnerException.Message.Contains("earlier BF2 1.5 renderer")&&e.InnerException.Message.Contains("https://community.pcgamingwiki.com/files/file/1031");}
  Check(rejected,"Specific earlier renderer receives conditional hotfix guidance and link");
  // Only create shortcuts inside this isolated fixture, never the user's Desktop.
  var make=a.GetType("BF2VR.InstallEngine").GetMethod("MakeShortcut",BindingFlags.Static|BindingFlags.NonPublic);
  foreach(string mod in new[]{"bf2","DZB2"}){
   string file=Path.Combine(fixture,"check-"+mod+".lnk");make.Invoke(null,new object[]{file,g,mod,false});
   object shell=Activator.CreateInstance(Type.GetTypeFromProgID("WScript.Shell")),link=null;
   try{link=shell.GetType().InvokeMember("CreateShortcut",BindingFlags.InvokeMethod,null,shell,new object[]{file});var t=link.GetType();string target=(string)t.InvokeMember("TargetPath",BindingFlags.GetProperty,null,link,null);string parameters=(string)t.InvokeMember("Arguments",BindingFlags.GetProperty,null,link,null);Check(target.EndsWith(mod=="bf2"?"BF2-VR.exe":"BF2-VR-DZB2.exe",StringComparison.OrdinalIgnoreCase),mod+" shortcut target");Check(mod=="DZB2"?parameters=="":parameters=="+menu 1 +modPath mods/bf2 +restart",mod+" shortcut arguments");}
   finally{if(link!=null)System.Runtime.InteropServices.Marshal.FinalReleaseComObject(link);System.Runtime.InteropServices.Marshal.FinalReleaseComObject(shell);}
  }
  Console.WriteLine("Final package inspection complete; no game or installation launched.");return 0;
 }catch(Exception e){Console.Error.WriteLine(e);return 1;}}
}
