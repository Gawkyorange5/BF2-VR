#if BF2VR_COMPATIBILITY_TOOL
using System;
using System.IO;
using System.Drawing;
using System.Windows.Forms;
using System.Threading.Tasks;

namespace BF2VR {
 internal sealed class CompatibilityForm:Form {
  readonly TextBox folder=new TextBox{Dock=DockStyle.Fill};
  readonly TextBox report=new TextBox{Dock=DockStyle.Fill,Multiline=true,ReadOnly=true,ScrollBars=ScrollBars.Both,WordWrap=false,Font=new Font("Consolas",9)};
  readonly Button browse=new Button{Text="Browse...",Dock=DockStyle.Fill};
  readonly Button save=new Button{Text="Create and save report",AutoSize=true};
  bool busy;
  public CompatibilityForm(){
   Text="BF2-VR compatibility report";ClientSize=new Size(820,620);MinimumSize=Size;StartPosition=FormStartPosition.CenterScreen;Font=new Font("Segoe UI",10);AutoScaleMode=AutoScaleMode.Dpi;
   var layout=new TableLayoutPanel{Dock=DockStyle.Fill,Padding=new Padding(16),ColumnCount=1,RowCount=4};Controls.Add(layout);
   layout.RowStyles.Add(new RowStyle(SizeType.Absolute,85));layout.RowStyles.Add(new RowStyle(SizeType.Absolute,40));layout.RowStyles.Add(new RowStyle(SizeType.Absolute,45));layout.RowStyles.Add(new RowStyle(SizeType.Percent,100));
   layout.Controls.Add(new Label{Dock=DockStyle.Fill,Text="Select the installed Battlefield 2 folder containing BF2.exe and RendDX9.dll.\r\nThen save the report and send the TXT to the mod developer.\r\nThis reads file versions and fingerprints. It does not install or change the game."});
   var row=new TableLayoutPanel{Dock=DockStyle.Fill,ColumnCount=2};row.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));row.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,110));row.Controls.Add(folder);row.Controls.Add(browse);layout.Controls.Add(row);
   var buttons=new FlowLayoutPanel{Dock=DockStyle.Fill};buttons.Controls.Add(save);layout.Controls.Add(buttons);layout.Controls.Add(report);
   if(File.Exists(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"BF2.exe")))folder.Text=AppDomain.CurrentDomain.BaseDirectory;
   browse.Click+=delegate{using(var dialog=new FolderBrowserDialog{Description="Select installed Battlefield 2, not the DVD"}){if(Directory.Exists(folder.Text))dialog.SelectedPath=folder.Text;if(dialog.ShowDialog(this)==DialogResult.OK)folder.Text=dialog.SelectedPath;}};
   save.Click+=async delegate{await SaveReport();};
   FormClosing+=delegate(object sender,FormClosingEventArgs e){if(busy)e.Cancel=true;};
  }
  async Task SaveReport(){
   string game=folder.Text.Trim();
   if(!Directory.Exists(game)||!File.Exists(Path.Combine(game,"BF2.exe"))){MessageBox.Show(this,"Select the installed game folder containing BF2.exe first.");return;}
   using(var dialog=new SaveFileDialog{Title="Save report to send to the developer",FileName="BF2-VR-Compatibility.txt",Filter="Text report (*.txt)|*.txt",DefaultExt="txt",AddExtension=true}){
    if(dialog.ShowDialog(this)!=DialogResult.OK)return;
    string output=Path.GetFullPath(dialog.FileName);
    if(!String.Equals(Path.GetExtension(output),".txt",StringComparison.OrdinalIgnoreCase)){MessageBox.Show(this,"Save the report as a .txt file.");return;}
    busy=true;save.Enabled=browse.Enabled=folder.Enabled=false;UseWaitCursor=true;
    try{string text=await Task.Run(delegate{return PeImage.CompatibilityReport(game);});report.Text=text;File.WriteAllText(output,text,System.Text.Encoding.UTF8);MessageBox.Show(this,"Report saved. Send this file, your game edition, and whether BF2 runs flat-screen as version 1.5:\r\n\r\n"+output,"BF2-VR");}
    catch(Exception e){MessageBox.Show(this,"Could not save the report: "+e.Message,"BF2-VR",MessageBoxButtons.OK,MessageBoxIcon.Warning);}
    finally{busy=false;save.Enabled=browse.Enabled=folder.Enabled=true;UseWaitCursor=false;}
   }
  }
 }
 internal static class CompatibilityProgram {
   [STAThread] static void Main(){Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);Application.Run(new CompatibilityForm());}
  }
}
#endif
