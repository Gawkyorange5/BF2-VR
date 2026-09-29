using System;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Web.Script.Serialization;

namespace BF2VR {
 public sealed class AudioRecipe { public int id,start,count; public string weapon,entry,sha256; public double gain; public bool gainFirst; }
 public static class ReloadAudioBuilder {
  [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr BeginUpdateResource(string file,bool deleteExisting);
  [DllImport("kernel32.dll",SetLastError=true)] static extern bool UpdateResource(IntPtr h,IntPtr type,IntPtr name,ushort language,byte[] data,uint size);
  [DllImport("kernel32.dll",SetLastError=true)] static extern bool EndUpdateResource(IntPtr h,bool discard);
  static Exception Error(){return new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());}
  public static double[] Decode(byte[] raw){
   using(var stream=new MemoryStream(raw)) using(var r=new BinaryReader(stream)){
    if(new string(r.ReadChars(4))!="RIFF")throw new InvalidDataException("Invalid WAV.");r.ReadUInt32();if(new string(r.ReadChars(4))!="WAVE")throw new InvalidDataException("Invalid WAV.");
    int channels=0,rate=0,bits=0,format=0;byte[] pcm=null;
    while(stream.Position+8<=stream.Length){string type=new string(r.ReadChars(4));int size=checked((int)r.ReadUInt32());long next=stream.Position+size+(size&1);if(next>stream.Length+1)throw new InvalidDataException("Truncated WAV.");
     if(type=="fmt "){if(size<16)throw new InvalidDataException("Invalid WAV format.");format=r.ReadUInt16();channels=r.ReadUInt16();rate=r.ReadInt32();r.ReadUInt32();r.ReadUInt16();bits=r.ReadUInt16();}
     else if(type=="data"){if(size>20000000)throw new InvalidDataException("Audio too large.");pcm=r.ReadBytes(size);}
     stream.Position=Math.Min(next,stream.Length);
    }
    if(format!=1 || bits!=16 || channels<1 || channels>8 || (rate!=22050 && rate!=44100) || pcm==null || pcm.Length%(channels*2)!=0)throw new InvalidDataException("Expected original PCM16 22.05/44.1 kHz reload audio.");
    var x=new double[pcm.Length/(channels*2)];for(int i=0;i<x.Length;i++){for(int c=0;c<channels;c++)x[i]+=BitConverter.ToInt16(pcm,(i*channels+c)*2);x[i]/=channels;}
    if(rate==22050)return x;
    var k=new double[63];double sum=0;
    for(int i=0;i<63;i++){int t=i-31;double sinc=t==0?1:Math.Sin(Math.PI*.45*t)/(Math.PI*.45*t);k[i]=.45*sinc*(.54-.46*Math.Cos(2*Math.PI*i/62));sum+=k[i];}
    for(int i=0;i<63;i++)k[i]/=sum;
    var y=new double[(x.Length+1)/2];for(int i=0;i<y.Length;i++)for(int j=0;j<63;j++){int p=i*2+31-j;if(p>=0 && p<x.Length)y[i]+=x[p]*k[j];}return y;
   }
  }
  public static byte[] Cut(double[] x,AudioRecipe a){
   if(a.entry=="")return new byte[2];
   if(a.count<1 || a.count>22050 || a.start<0 || a.start>x.Length-a.count)throw new InvalidDataException("Reload sound differs from the supported original: "+a.weapon);
   var b=new byte[a.count*2];int fade=Math.Min(110,a.count/2);
   for(int i=0;i<a.count;i++){double v=x[a.start+i];if(a.gainFirst)v*=a.gain;
    if(fade>1){if(i<fade)v*=(double)i/(fade-1);if(i>=a.count-fade)v*=(double)(a.count-1-i)/(fade-1);}
    if(!a.gainFirst)v*=a.gain;
    short n=(short)Math.Max(-32768,Math.Min(32767,Math.Round(v,MidpointRounding.ToEven)));b[i*2]=(byte)n;b[i*2+1]=(byte)(n>>8);
   }return b;
  }
  public static Dictionary<int,byte[]> Generate(string zipPath,string recipes){
   var items=new JavaScriptSerializer().Deserialize<AudioRecipe[]>(recipes);if(items.Length!=116)throw new InvalidDataException("Audio manifest is incomplete.");
   var result=new Dictionary<int,byte[]>();var cache=new Dictionary<string,double[]>(StringComparer.OrdinalIgnoreCase);
   using(var z=ZipFile.OpenRead(zipPath)){
    var entries=z.Entries.ToDictionary(e=>e.FullName,StringComparer.OrdinalIgnoreCase);
    foreach(var a in items){double[] x=new double[0];if(a.entry!="" && !cache.TryGetValue(a.entry,out x)){
      ZipArchiveEntry e;if(!entries.TryGetValue(a.entry,out e))throw new FileNotFoundException("Missing original reload audio: "+a.entry+". Use a complete BF2 1.5 installation.");
      if(e.Length>20000000)throw new InvalidDataException("Unexpected audio size.");using(var m=new MemoryStream()){using(var s=e.Open())s.CopyTo(m);x=Decode(m.ToArray());}cache[a.entry]=x;
     }result.Add(a.id,Cut(x,a));
    }
   }return result;
  }
  public static void Embed(string dll,Dictionary<int,byte[]> cues){
   IntPtr h=BeginUpdateResource(dll,false);if(h==IntPtr.Zero)throw Error();bool closed=false;
   try{foreach(var p in cues){if(p.Key<1100 || p.Key>1215)throw new InvalidDataException("Invalid resource id.");if(!UpdateResource(h,(IntPtr)10,(IntPtr)p.Key,1033,p.Value,(uint)p.Value.Length))throw Error();}
    bool ok=EndUpdateResource(h,false);closed=true;if(!ok)throw Error();
   }finally{if(!closed)EndUpdateResource(h,true);}
  }
 }
}
