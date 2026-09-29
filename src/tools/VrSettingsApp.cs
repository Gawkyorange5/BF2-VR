using System;
using System.Collections.Generic;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows.Forms;
[assembly:System.Reflection.AssemblyTitle("Battlefield 2 VR Settings")]
[assembly:System.Reflection.AssemblyVersion("1.44.0.0")]
[assembly:System.Reflection.AssemblyFileVersion("1.44.0.0")]

internal static class Program {
    [STAThread] private static void Main(string[] args) {
        Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
        Application.SetUnhandledExceptionMode(UnhandledExceptionMode.CatchException);
        Application.ThreadException+=delegate(object sender,System.Threading.ThreadExceptionEventArgs e){Error(e.Exception);};
        try{Application.Run(new SettingsForm(args.Length>0?args[0]:null));}catch(Exception e){Error(e);}
    }
    internal static void Error(Exception e){MessageBox.Show(e.Message,"BF2 VR settings",MessageBoxButtons.OK,MessageBoxIcon.Error);}
}
internal sealed class Option {
    internal readonly int Page;
    internal readonly bool Toggle;
    internal readonly string Section,Key,Label,Help;
    internal readonly decimal Min,Max,Step,Default;
    internal string Raw;
    internal decimal Loaded;
    internal bool Invalid;
    internal CheckBox Check;
    internal NumericUpDown Number;
    internal decimal Value {get{return Toggle?(Check.Checked?1:0):Number.Value;}set{if(Toggle)Check.Checked=value!=0;else Number.Value=value;}}
    internal bool Dirty {get{return Invalid||Value!=Loaded;}}
    internal Option(int page,bool toggle,string section,string key,string label,string help,decimal min,decimal max,decimal step,decimal initial){
        Page=page;Toggle=toggle;Section=section;Key=key;Label=label;Help=help;Min=min;Max=max;Step=step;Default=initial;
    }
}
internal sealed class SettingsForm : Form {
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode)]
    private static extern uint GetPrivateProfileString(string section,string key,string fallback,StringBuilder value,uint size,string path);
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]
    [return:MarshalAs(UnmanagedType.Bool)]
    private static extern bool WritePrivateProfileString(string section,string key,string value,string path);
    private readonly Option[] options=SettingsCatalog.Options();
    private readonly HashSet<string> backups=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
    private readonly TabControl tabs=new TabControl{Dock=DockStyle.Fill};
    private Label file,status;
    private Button apply,reload;
    private string ini;
    internal SettingsForm(string path){
        Text="Battlefield 2 VR Settings - r44";Font=new Font("Segoe UI",10);AutoScaleMode=AutoScaleMode.Dpi;
        ClientSize=new Size(690,690);MinimumSize=new Size(630,520);StartPosition=FormStartPosition.CenterScreen;
        TableLayoutPanel root=new TableLayoutPanel{Dock=DockStyle.Fill,ColumnCount=1,RowCount=4,Padding=new Padding(16)};
        root.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));root.RowStyles.Add(new RowStyle(SizeType.AutoSize));root.RowStyles.Add(new RowStyle(SizeType.Percent,100));
        root.RowStyles.Add(new RowStyle(SizeType.AutoSize));root.RowStyles.Add(new RowStyle(SizeType.AutoSize));Controls.Add(root);
        TableLayoutPanel top=MakeLayout();file=TextLabel(top,"Choose your game folder to get started.",false);
        FlowLayoutPanel browse=new FlowLayoutPanel{AutoSize=true,Dock=DockStyle.Top};
        Button choose=MakeButton("Choose game...");choose.Click+=delegate{Choose();};browse.Controls.Add(choose);
        reload=MakeButton("Reload saved values");reload.Enabled=false;reload.Click+=delegate{if(CanDiscard())LoadFolder(Path.GetDirectoryName(ini));};browse.Controls.Add(reload);top.Controls.Add(browse);
        TextLabel(top,"In the headset: hold both joystick clicks for these settings. Close to recenter.\nTap left menu for kits; hold left menu for pause.",false);
        root.Controls.Add(top,0,0);root.Controls.Add(tabs,0,1);
        string[] names={"Turning","HUD","Body / arms","Vehicles","Scopes"};
        for(int p=0;p<names.Length;++p){
            TabPage page=new TabPage(names[p]){AutoScroll=true,Padding=new Padding(12)};TableLayoutPanel layout=MakeLayout();page.Controls.Add(layout);tabs.TabPages.Add(page);
            foreach(Option o in options)if(o.Page==p)AddOption(layout,o);
        }
        FlowLayoutPanel actions=new FlowLayoutPanel{AutoSize=true,Dock=DockStyle.Top,FlowDirection=FlowDirection.RightToLeft,Padding=new Padding(0,10,0,0)};
        apply=MakeButton("Apply");apply.Enabled=false;apply.Click+=delegate{Save();};actions.Controls.Add(apply);
        Button defaults=MakeButton("Reset this tab");defaults.Click+=delegate{foreach(Option o in options)if(o.Page==tabs.SelectedIndex)o.Value=o.Default;status.Text="Defaults selected for this tab. Click Apply to save.";};actions.Controls.Add(defaults);root.Controls.Add(actions,0,2);
        status=new Label{AutoSize=true,MaximumSize=new Size(610,0),MinimumSize=new Size(0,48),Dock=DockStyle.Top,Margin=new Padding(3,8,3,0),Text="Settings are saved only when you click Apply."};root.Controls.Add(status,0,3);
        FormClosing+=delegate(object sender,FormClosingEventArgs e){if(!CanDiscard())e.Cancel=true;};
        if(String.IsNullOrWhiteSpace(path))path=AppDomain.CurrentDomain.BaseDirectory;
        if(File.Exists(path)&&String.Equals(Path.GetFileName(path),"BF2.exe",StringComparison.OrdinalIgnoreCase))path=Path.GetDirectoryName(path);
        if(Directory.Exists(path)&&File.Exists(Path.Combine(path,"BF2.exe")))LoadFolder(path);
    }
    private static TableLayoutPanel MakeLayout(){TableLayoutPanel t=new TableLayoutPanel{AutoSize=true,Dock=DockStyle.Top,ColumnCount=1};t.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));return t;}
    private static Button MakeButton(string text){return new Button{Text=text,AutoSize=true,MinimumSize=new Size(110,34),Padding=new Padding(8,0,8,0)};}
    private static Label TextLabel(TableLayoutPanel parent,string text,bool bold){
        Label l=new Label{Text=text,AutoSize=true,MaximumSize=new Size(580,0),Dock=DockStyle.Top,Margin=new Padding(0,bold?16:4,0,8)};
        if(bold)l.Font=new Font("Segoe UI",10,FontStyle.Bold);parent.Controls.Add(l);return l;
    }
    private void AddOption(TableLayoutPanel parent,Option o){
        if(o.Toggle){o.Check=new CheckBox{Text=o.Label,AutoSize=true,Margin=new Padding(0,16,0,4)};parent.Controls.Add(o.Check);}
        else{
            TextLabel(parent,o.Label,true);
            TableLayoutPanel row=new TableLayoutPanel{AutoSize=true,Dock=DockStyle.Top,ColumnCount=2};row.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));row.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,114));
            o.Number=new NumericUpDown{Minimum=o.Min,Maximum=o.Max,DecimalPlaces=o.Step<1?2:1,Increment=o.Step,Dock=DockStyle.Top,AccessibleName=o.Label};
            TrackBar slider=new TrackBar{Minimum=0,Maximum=1000,TickStyle=TickStyle.None,Dock=DockStyle.Fill,Height=38,AccessibleName=o.Label};
            row.Controls.Add(slider,0,0);row.Controls.Add(o.Number,1,0);parent.Controls.Add(row);
            Label detail=TextLabel(parent,"",false);bool syncing=false;
            EventHandler update=delegate{
                if(syncing)return;syncing=true;
                try{slider.Value=(int)Math.Round((o.Value-o.Min)/(o.Max-o.Min)*1000);
                    detail.Text=o.Key=="turn_speed"?String.Format("{0:0.00}x default input rate (240). Higher turns faster.",o.Value/240):"Default: "+o.Default.ToString(CultureInfo.CurrentCulture);}
                finally{syncing=false;}
            };
            o.Number.ValueChanged+=update;
            slider.ValueChanged+=delegate{if(syncing)return;decimal v=o.Min+(o.Max-o.Min)*slider.Value/1000;v=Math.Round(v/o.Step)*o.Step;o.Value=Math.Max(o.Min,Math.Min(o.Max,v));};
            o.Value=o.Default;update(null,EventArgs.Empty);
        }
        o.Value=o.Default;TextLabel(parent,o.Help,false);o.Loaded=o.Value;
    }
    private bool CanDiscard(){foreach(Option o in options)if(o.Value!=o.Loaded)return MessageBox.Show(this,"Discard your unapplied changes?","BF2 VR settings",MessageBoxButtons.YesNo,MessageBoxIcon.Question)==DialogResult.Yes;return true;}
    private void Choose(){using(OpenFileDialog d=new OpenFileDialog()){d.Title="Select Battlefield 2's BF2.exe";d.Filter="Battlefield 2 (BF2.exe)|BF2.exe";if(ini!=null)d.InitialDirectory=Path.GetDirectoryName(ini);if(d.ShowDialog(this)==DialogResult.OK&&CanDiscard())LoadFolder(Path.GetDirectoryName(d.FileName));}}
    private string Read(Option o){const string missing="__BF2_VR_MISSING__";StringBuilder b=new StringBuilder(1024);GetPrivateProfileString(o.Section,o.Key,missing,b,1024,ini);return b.ToString()==missing?null:b.ToString();}
    private void LoadFolder(string path){
        path=Path.GetFullPath(path);if(!File.Exists(Path.Combine(path,"BF2.exe")))throw new IOException("The selected folder does not contain BF2.exe.");
        ini=Path.Combine(path,"vr_stereo.ini");file.Text="Settings file: "+ini;List<string> invalid=new List<string>();
        foreach(Option o in options){
            o.Raw=Read(o);decimal value=o.Default;
            o.Invalid=o.Raw!=null&&(!Decimal.TryParse(o.Raw,NumberStyles.Float,CultureInfo.InvariantCulture,out value)||value<o.Min||value>o.Max||(o.Toggle&&value!=0&&value!=1));
            if(o.Invalid){value=o.Default;invalid.Add(o.Key);}o.Value=value;o.Loaded=o.Value;
        }
        apply.Enabled=reload.Enabled=true;status.Text=invalid.Count==0?"Ready. Apply saves only changed settings, with an INI backup.":"Invalid value in "+String.Join(", ",invalid.ToArray())+". Showing defaults; choose a value and Apply to repair.";
    }
    private void Save(){try{
        if(ini==null)throw new IOException("Choose your game folder first.");List<Option> changed=new List<Option>();
        foreach(Option o in options)if(o.Dirty){if(Read(o)!=o.Raw)throw new IOException(o.Key+" changed outside this window. Use Reload saved values before editing it again.");changed.Add(o);}
        if(changed.Count==0){status.Text="No changes to save.";return;}
        if(!backups.Contains(ini)){if(File.Exists(ini))File.Copy(ini,ini+".before-r44-settings-"+DateTime.Now.ToString("yyyyMMdd-HHmmssfff")+".bak",false);backups.Add(ini);}
        foreach(Option o in changed){string value=o.Value.ToString("0.####",CultureInfo.InvariantCulture);
            if(!WritePrivateProfileString(o.Section,o.Key,value,ini))throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error(),"Could not save "+o.Key+". Check game folder write access.");
            o.Raw=value;o.Loaded=o.Value;o.Invalid=false;}
        status.Text="Saved. Return to BF2; settings reload within half a second on r44+.";
    }catch(Exception e){Program.Error(e);}}
}
