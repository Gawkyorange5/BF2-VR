using System;using System.IO;using System.Text;using System.Threading;
namespace BF2VR {
 internal sealed class MaintenanceGate:IDisposable{
  Mutex mutex;bool owned;
  public MaintenanceGate(string folder){string key=PeImage.Hash(Encoding.UTF8.GetBytes(Path.GetFullPath(folder).TrimEnd('\\').ToLowerInvariant()));mutex=new Mutex(false,"Local\\BF2VR-Install-"+key);try{owned=mutex.WaitOne(0);}catch(AbandonedMutexException){owned=true;}if(!owned){mutex.Dispose();throw new IOException("Another installation or DZB2 session is using this game folder. Close it and try again.");}}
  public void Dispose(){if(owned){mutex.ReleaseMutex();owned=false;}mutex.Dispose();}
 }
}
