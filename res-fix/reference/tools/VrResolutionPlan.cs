using System;
using System.Globalization;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

// One policy for normal BF2 startup and the settings app's probe helper.
// The parent game bounds this child process to 15 seconds.
internal static class VrResolutionPlan {
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode)]
    static extern uint GetPrivateProfileString(string section,string key,string fallback,StringBuilder value,uint size,string path);
    static decimal Number(string folder,string key,decimal fallback,decimal min,decimal max){
        var text=new StringBuilder(64);GetPrivateProfileString("graphics",key,fallback.ToString(CultureInfo.InvariantCulture),text,64,Path.Combine(folder,"vr_stereo.ini"));
        decimal value;
        return Decimal.TryParse(text.ToString(),NumberStyles.Float,CultureInfo.InvariantCulture,out value)&&value>=min&&value<=max?value:fallback;
    }
    internal static string Resolve(string folder){
        int mode=(int)Number(folder,"resolution_mode",0m,0m,2m);
        if(mode==2)throw new IOException("Game resolution was selected while the headset query was starting. Restart BF2.");
        decimal percent=mode==0?100m:Number(folder,"eye_resolution_percent",100m,25m,150m);
        // Always ask the CURRENT connected runtime; never use yesterday's INI.
        string[] measured=VrResolution.Probe(folder).Split('|');
        int rw,rh,mw,mh;
        if(measured.Length!=5||!Int32.TryParse(measured[0],out rw)||!Int32.TryParse(measured[1],out rh)||
           !Int32.TryParse(measured[3],out mw)||!Int32.TryParse(measured[4],out mh))
            throw new IOException("The OpenXR resolution response was invalid.");
        int w=(int)Math.Ceiling(rw*percent/100m),h=(int)Math.Ceiling(rh*percent/100m);
        int maxW=Math.Min(mw,4096),maxH=Math.Min(mh,4096);
        const long pixelLimit=8388608; // Existing 32-bit allocation ceiling.
        bool limited=w>maxW||h>maxH||(long)w*h>pixelLimit;
        if(limited){
            double scale=Math.Min(Math.Min((double)maxW/w,(double)maxH/h),Math.Sqrt((double)pixelLimit/((long)w*h)));
            w=(int)Math.Floor(w*scale);h=(int)Math.Floor(h*scale);
        }
        if(w<320||h<320||w>maxW||h>maxH||(long)w*h>pixelLimit)
            throw new IOException("The headset requested an unsupported render size.");
        return String.Format(CultureInfo.InvariantCulture,"OK59|{0}|{1}|{2}|{3}|{4}|{5}",w,h,rw,rh,mode,limited?1:0);
    }
}
