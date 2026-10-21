using System;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

// Query the same 32-bit OpenXR loader/runtime used by BF2. No XR session,
// graphics device, settings change or game process is created by this probe.
internal static class VrResolution {
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr LoadLibraryEx(string path,IntPtr file,uint flags);
    [DllImport("kernel32.dll")] static extern IntPtr GetProcAddress(IntPtr module,string name);
    [DllImport("kernel32.dll")] static extern bool FreeLibrary(IntPtr module);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate int Create(IntPtr info,out ulong instance);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate int Destroy(ulong instance);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate int SystemId(ulong instance,IntPtr info,out ulong system);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate int Views(ulong instance,ulong system,int config,uint capacity,out uint count,IntPtr views);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate int Properties(ulong instance,IntPtr properties);
    static T Proc<T>(IntPtr library,string name) where T:class {
        IntPtr p=GetProcAddress(library,name);if(p==IntPtr.Zero)throw new IOException("OpenXR loader is missing "+name);
        return Marshal.GetDelegateForFunctionPointer(p,typeof(T)) as T;
    }
    static IntPtr Buffer(int size,int type){IntPtr p=Marshal.AllocHGlobal(size);Marshal.Copy(new byte[size],0,p,size);Marshal.WriteInt32(p,type);return p;}
    static void Check(int result,string operation){if(result<0)throw new IOException(operation+" failed (OpenXR "+result+"). Connect the headset and start its PC VR connection first.");}
    internal static string Probe(string folder){
        if(IntPtr.Size!=4)throw new IOException("The OpenXR resolution probe must run as 32-bit, like BF2.");
        IntPtr lib=LoadLibraryEx(Path.Combine(folder,"openxr_loader.dll"),IntPtr.Zero,8);
        if(lib==IntPtr.Zero)throw new IOException("Could not load the installed 32-bit OpenXR loader. Windows error "+Marshal.GetLastWin32Error());
        ulong instance=0;IntPtr ci=IntPtr.Zero,si=IntPtr.Zero,vi=IntPtr.Zero,pi=IntPtr.Zero;
        try{
            // OpenXR x86 ABI; handles and versions are uint64, pointers uint32.
            ci=Buffer(304,3);byte[] name=Encoding.ASCII.GetBytes("BF2 VR resolution query\0");Marshal.Copy(name,0,IntPtr.Add(ci,16),name.Length);
            Marshal.WriteInt32(ci,144,49);Marshal.WriteInt64(ci,280,1L<<48); // OpenXR 1.0
            Check(Proc<Create>(lib,"xrCreateInstance")(ci,out instance),"Create OpenXR instance");
            si=Buffer(12,4);Marshal.WriteInt32(si,8,1);ulong system;
            Check(Proc<SystemId>(lib,"xrGetSystem")(instance,si,out system),"Find headset");
            var views=Proc<Views>(lib,"xrEnumerateViewConfigurationViews");uint count;
            Check(views(instance,system,2,0,out count,IntPtr.Zero),"Query stereo views");
            if(count!=2)throw new IOException("The active runtime did not report two stereo eye views.");
            vi=Buffer(64,41);Marshal.WriteInt32(vi,32,41);
            Check(views(instance,system,2,2,out count,vi),"Read eye resolution");
            if(count!=2)throw new IOException("The active runtime changed its stereo view count during the query.");
            int w=Math.Max(Marshal.ReadInt32(vi,8),Marshal.ReadInt32(vi,40));
            int h=Math.Max(Marshal.ReadInt32(vi,16),Marshal.ReadInt32(vi,48));
            if(w<320||h<320||w>16384||h>16384)throw new IOException("The runtime returned an invalid eye size.");
            pi=Buffer(144,32);Check(Proc<Properties>(lib,"xrGetInstanceProperties")(instance,pi),"Read runtime name");
            string runtime=Marshal.PtrToStringAnsi(IntPtr.Add(pi,16))??"Unknown";
            int maxW=Math.Min(Marshal.ReadInt32(vi,12),Marshal.ReadInt32(vi,44));
            int maxH=Math.Min(Marshal.ReadInt32(vi,20),Marshal.ReadInt32(vi,52));
            if(maxW<320||maxH<320)throw new IOException("The runtime reported invalid maximum eye dimensions.");
            return w.ToString(CultureInfo.InvariantCulture)+"|"+h.ToString(CultureInfo.InvariantCulture)+"|"+runtime+"|"+maxW+"|"+maxH;
        }finally{
            if(instance!=0)Proc<Destroy>(lib,"xrDestroyInstance")(instance);
            foreach(IntPtr p in new[]{ci,si,vi,pi})if(p!=IntPtr.Zero)Marshal.FreeHGlobal(p);
            FreeLibrary(lib);
        }
    }
    internal static string Query(string folder){
        // Isolate runtime startup: a disconnected/broken runtime must not hang
        // the settings app or cause yesterday's recommendation to be reused.
        string result=Path.Combine(Path.GetTempPath(),"bf2-vr-eye-"+Guid.NewGuid().ToString("N")+".txt");
        try{
            string exe=System.Reflection.Assembly.GetExecutingAssembly().Location;
            using(var p=Process.Start(new ProcessStartInfo{FileName=exe,Arguments="--probe-openxr \""+folder.TrimEnd('\\')+"\" \""+result+"\"",WorkingDirectory=folder,UseShellExecute=false,CreateNoWindow=true})){
                if(!p.WaitForExit(15000)){p.Kill();throw new IOException("The OpenXR runtime did not answer within 15 seconds. Reconnect PC VR and try again. No game was launched.");}
                string output=File.Exists(result)?File.ReadAllText(result):"OpenXR probe stopped without returning a result.";
                if(p.ExitCode!=0)throw new IOException(output);return output;
            }
        }finally{if(File.Exists(result))File.Delete(result);}
    }
}
