using System;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using Microsoft.Win32;

// BF2's own +szx/+szy startup options resize the native renderer and all its
// dependent targets. Merely changing the OpenXR texture would upscale pixels.
internal static class VrLaunch {
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode)]
    private static extern uint GetPrivateProfileString(string section,string key,string fallback,StringBuilder value,uint size,string path);
    private static string Read(string path,string section,string key,string fallback){
        var value=new StringBuilder(512);GetPrivateProfileString(section,key,fallback,value,512,path);return value.ToString();
    }
    internal static string Arguments(string folder){
        // r59 resolves the current headset before BF2 reads its startup options.
        // No stale saved dimensions and no special launch button requirement.
        return "+menu 1 +modPath mods/bf2";
    }

    internal static void Start(string folder){
        folder=Path.GetFullPath(folder);
        foreach(var p in Process.GetProcesses())using(p){try{
            if(p.ProcessName.Equals("BF2",StringComparison.OrdinalIgnoreCase)||p.ProcessName.Equals("BF2-VR",StringComparison.OrdinalIgnoreCase))
                throw new IOException("Exit Battlefield 2 before launching with a different eye resolution.");
        }catch(InvalidOperationException){}}
        if(!File.Exists(Path.Combine(folder,"RFX.dll")))throw new IOException("Choose your installed BF2 VR folder.");
        string args=Arguments(folder),exe=Path.Combine(folder,"BF2-VR.exe");
        // Settings save booleans as either 1 or 1.0000.
        decimal steamValue;bool steam=Decimal.TryParse(Read(Path.Combine(folder,"vr_stereo.ini"),"graphics","steam_launch","0"),NumberStyles.Float,CultureInfo.InvariantCulture,out steamValue)&&steamValue==1;
        if(steam){
            if(!File.Exists(Path.Combine(folder,"dinput8.dll")))throw new IOException("The Steam launch option needs the private-preview Steam loader installed first. Use the r46 Steam fix with the original BF2.exe.");
            exe=Registry.GetValue(@"HKEY_CURRENT_USER\Software\Valve\Steam","SteamExe",null) as string;
            if(String.IsNullOrEmpty(exe)||!File.Exists(exe))throw new IOException("Steam.exe was not found. Open Steam once, then try Launch VR again.");
            args="-applaunch 24860 "+args;
        }
        if(!File.Exists(exe))throw new IOException("BF2-VR.exe was not found. Select your installed game folder, or enable Launch through Steam for the Steam edition with its loader.");
        // Record only the public launch parameters, never account details.
        File.WriteAllText(Path.Combine(folder,"bf2_vr_launch.log"),DateTime.Now.ToString("s")+Environment.NewLine+"Launch: "+(steam?"Steam Battlefield 2":"BF2-VR.exe")+Environment.NewLine+args+Environment.NewLine);
        Process.Start(new ProcessStartInfo{FileName=exe,Arguments=args,WorkingDirectory=folder,UseShellExecute=false});
    }
}
