using System;
using System.IO;
using System.Linq;
using System.Drawing;
using System.Windows.Forms;
using System.Threading.Tasks;
using Microsoft.Win32;
using System.Diagnostics;

namespace BF2VR {
 public sealed class SetupForm:Form {
  TextBox folder=new TextBox(), log=new TextBox();ComboBox profile=new ComboBox(),mod=new ComboBox();
  CheckBox bindings=new CheckBox(),shortcut=new CheckBox(),ready=new CheckBox();Button install=new Button(),remove=new Button(),browse=new Button();
  Label status=new Label();bool busy;
  public SetupForm(){
   Text="BF2-VR Setup · r45 public baseline";ClientSize=new Size(730,650);MinimumSize=Size;StartPosition=FormStartPosition.CenterScreen;Font=new Font("Segoe UI",10);AutoScaleMode=AutoScaleMode.Dpi;
   var title=new Label{Text="Battlefield 2 VR",Font=new Font("Segoe UI",20,FontStyle.Bold),AutoSize=true,Location=new Point(20,15)};Controls.Add(title);
   AddText("1. Patch Battlefield 2 to 1.5 and play it successfully flat-screen first.\n2. Connect your headset and select its OpenXR runtime.\n3. Close BF2, select the game folder and your profile, then install.",20,65,690,70);
   AddText("Battlefield 2 folder (contains BF2.exe)",20,143,600,25);folder.SetBounds(20,171,595,28);Controls.Add(folder);browse.Text="Browse…";browse.SetBounds(625,169,85,30);Controls.Add(browse);
   browse.Click+=delegate{using(var d=new FolderBrowserDialog{Description="Choose the Battlefield 2 game folder",SelectedPath=folder.Text})if(d.ShowDialog()==DialogResult.OK){folder.Text=d.SelectedPath;RefreshMods();}};
   AddText("BF2 profile for controller bindings",20,210,310,25);profile.SetBounds(20,238,310,30);profile.DropDownStyle=ComboBoxStyle.DropDownList;Controls.Add(profile);
   foreach(string p in InstallEngine.Profiles())profile.Items.Add(Path.GetFileName(p));if(profile.Items.Count>0)profile.SelectedIndex=0;
   AddText("Desktop shortcut game/mod",365,210,340,25);mod.SetBounds(365,238,345,30);mod.DropDownStyle=ComboBoxStyle.DropDownList;Controls.Add(mod);
   bindings.Text="Apply recommended controller bindings (backs up Controls.con)";bindings.Checked=profile.Items.Count>0;bindings.SetBounds(20,281,690,27);Controls.Add(bindings);
   shortcut.Text="Create a Battlefield 2 VR desktop shortcut";shortcut.Checked=true;shortcut.SetBounds(20,312,690,27);Controls.Add(shortcut);
   ready.Text="I have patched to 1.5 and played BF2 flat-screen successfully.";ready.SetBounds(20,347,690,28);Controls.Add(ready);
   AddText("Setup creates BF2-VR.exe from your own executable and builds reload sounds locally.\nYour original BF2.exe and existing VR preferences are preserved. No game files are downloaded.",20,382,690,48);
   install.Text="Install BF2-VR";install.SetBounds(20,444,190,36);install.Enabled=false;Controls.Add(install);
   remove.Text="Remove / Restore";remove.SetBounds(220,444,180,36);Controls.Add(remove);
   var help=new LinkLabel{Text="Setup guide / controls",AutoSize=true,Location=new Point(430,452)};Controls.Add(help);help.LinkClicked+=delegate{ShowGuide();};
   ready.CheckedChanged+=delegate{install.Enabled=ready.Checked&&!busy;};
   install.Click+=async delegate{await Run(false);};remove.Click+=async delegate{if(MessageBox.Show(this,"Restore the files backed up by BF2-VR Setup in the selected game folder? Files changed since installation will be preserved.","Remove BF2-VR",MessageBoxButtons.YesNo,MessageBoxIcon.Question)==DialogResult.Yes)await Run(true);};
   log.SetBounds(20,495,690,125);log.Multiline=true;log.ReadOnly=true;log.ScrollBars=ScrollBars.Vertical;log.Anchor=AnchorStyles.Left|AnchorStyles.Right|AnchorStyles.Top|AnchorStyles.Bottom;Controls.Add(log);
   status.SetBounds(20,624,690,22);status.Text="OpenXR · no mandatory SteamVR · Windows 10/11";status.Anchor=AnchorStyles.Bottom|AnchorStyles.Left;Controls.Add(status);
   folder.Leave+=delegate{RefreshMods();};FormClosing+=delegate(object sender,FormClosingEventArgs e){if(busy){e.Cancel=true;MessageBox.Show(this,"Please wait for installation or restoration to finish.");}};
   folder.Text=FindGame();RefreshMods();Append("Choose your game folder. Keep the downloaded Setup executable to remove the mod later.");
  }
  void AddText(string text,int x,int y,int w,int h){Controls.Add(new Label{Text=text,Location=new Point(x,y),Size=new Size(w,h)});}
  void Append(string text){if(InvokeRequired){Invoke(new Action<string>(Append),text);return;}log.AppendText(text+Environment.NewLine);}
  void RefreshMods(){string previous=mod.Text;mod.Items.Clear();string d=Path.Combine(folder.Text,"mods");if(Directory.Exists(d))foreach(string p in Directory.GetDirectories(d))if(File.Exists(Path.Combine(p,"init.con")))mod.Items.Add(Path.GetFileName(p));if(mod.Items.Contains(previous))mod.SelectedItem=previous;else if(mod.Items.Contains("bf2"))mod.SelectedItem="bf2";else if(mod.Items.Count>0)mod.SelectedIndex=0;}
  static string FindGame(){foreach(string k in new[]{@"SOFTWARE\EA Games\Battlefield 2",@"SOFTWARE\Electronic Arts\EA Games\Battlefield 2"})using(var key=Registry.LocalMachine.OpenSubKey(k)){var p=key==null?null:key.GetValue("InstallDir") as string;if(p!=null&&File.Exists(Path.Combine(p,"BF2.exe")))return p;}string beside=AppDomain.CurrentDomain.BaseDirectory;return File.Exists(Path.Combine(beside,"BF2.exe"))?beside:"";}
  static string MissingDependencies(string game){var absent=new System.Collections.Generic.List<string>();foreach(string name in new[]{"d3dx9_43.dll","MSVCP140.dll","VCRUNTIME140.dll","MSVCP71.dll","MSVCR71.dll"})if(!File.Exists(Path.Combine(game,name))&&!File.Exists(Path.Combine(Environment.SystemDirectory,name)))absent.Add(name);return String.Join(", ",absent);}
  async Task Run(bool uninstall){string g=folder.Text.Trim();if(!Directory.Exists(g)){MessageBox.Show(this,"Choose an existing Battlefield 2 folder.");return;}
   string p=bindings.Checked&&profile.SelectedItem!=null?Path.Combine(InstallEngine.DocumentsProfiles(),profile.Text):null;
   if(!uninstall&&bindings.Checked&&p==null){MessageBox.Show(this,"Run BF2 flat-screen and create a local profile first, then reopen Setup.");return;}
   if(!uninstall){string missing=MissingDependencies(g);if(missing!=""){MessageBox.Show(this,"Required 32-bit runtime DLLs were not found: "+missing+"\n\nInstall Microsoft's Visual C++ x86 redistributable and DirectX June 2010 runtime as described in the guide. If BF2 itself is missing MSVCP71/MSVCR71, repair your game installation first.","Prerequisites missing");ShowGuide();return;}}
   string selectedMod=mod.Text;bool makeShortcut=shortcut.Checked;busy=true;install.Enabled=remove.Enabled=browse.Enabled=folder.Enabled=profile.Enabled=mod.Enabled=bindings.Enabled=shortcut.Enabled=ready.Enabled=false;
   try{await Task.Run(delegate{var engine=new InstallEngine{Log=Append};if(uninstall)engine.Uninstall(g);else engine.Install(g,p,makeShortcut,selectedMod);});MessageBox.Show(this,uninstall?"BF2-VR removed and previous files restored.":"BF2-VR installed.\n\nConnect your headset, then launch Battlefield 2 VR. Tap both stick clicks to recenter. Hold both to open VR settings.","BF2-VR");}
   catch(Exception e){Append(e.Message);MessageBox.Show(this,e.Message+"\n\nIf access is denied, close the game and rerun Setup as administrator for a protected game folder. Do not delete .bf2-vr; it contains your backups.","Setup stopped",MessageBoxButtons.OK,MessageBoxIcon.Warning);}
   finally{busy=false;remove.Enabled=browse.Enabled=folder.Enabled=profile.Enabled=mod.Enabled=bindings.Enabled=shortcut.Enabled=ready.Enabled=true;install.Enabled=ready.Checked;try{File.WriteAllText(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"BF2-VR-Setup.log"),log.Text);}catch{}}
  }
  void ShowGuide(){using(var s=System.Reflection.Assembly.GetExecutingAssembly().GetManifestResourceStream("payload.BF2-VR-README.txt"))using(var r=new StreamReader(s)){var f=new Form{Text="BF2-VR installation and controls",Size=new Size(850,700),StartPosition=FormStartPosition.CenterParent};var text=new RichTextBox{ReadOnly=true,DetectUrls=true,Dock=DockStyle.Fill,Font=new Font("Consolas",10),Text=r.ReadToEnd()};text.LinkClicked+=delegate(object sender,LinkClickedEventArgs e){Uri uri;if(Uri.TryCreate(e.LinkText,UriKind.Absolute,out uri)&&(uri.Scheme=="https"||uri.Scheme=="http"))Process.Start(new ProcessStartInfo(uri.AbsoluteUri){UseShellExecute=true});};f.Controls.Add(text);f.Shown+=delegate{text.Select(0,0);};f.ShowDialog(this);}}
 }
 static class Program{[STAThread]static void Main(){Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);Application.Run(new SetupForm());}}
}
