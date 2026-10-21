using System;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Web.Script.Serialization;
namespace BF2VR {
 public sealed class AudioRecipe {
  public int id,start,count,fade,filter,rate,phase;
  public string weapon,entry,archive,mod,sha256;
  public double gain;
  public bool gainFirst,truncate;
 }
 public static class ReloadAudioBuilder {
  [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr BeginUpdateResource(string file,bool deleteExisting);
  [DllImport("kernel32.dll",SetLastError=true)] static extern bool UpdateResource(IntPtr h,IntPtr type,IntPtr name,ushort language,byte[] data,uint size);
  [DllImport("kernel32.dll",SetLastError=true)] static extern bool EndUpdateResource(IntPtr h,bool discard);
  static Exception Error(){return new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());}
  public static double[] Decode(byte[] raw,AudioRecipe recipe){
   using(var stream=new MemoryStream(raw)) using(var r=new BinaryReader(stream)){
    if(new string(r.ReadChars(4))!="RIFF")throw new InvalidDataException("Invalid WAV.");r.ReadUInt32();if(new string(r.ReadChars(4))!="WAVE")throw new InvalidDataException("Invalid WAV.");
    int channels=0,rate=0,bits=0,format=0;byte[] pcm=null;
    while(stream.Position+8<=stream.Length){string type=new string(r.ReadChars(4));int size=checked((int)r.ReadUInt32());long next=stream.Position+size+(size&1);if(size<0||next>stream.Length+1)throw new InvalidDataException("Truncated WAV.");
     if(type=="fmt "){if(size<16)throw new InvalidDataException("Invalid WAV format.");format=r.ReadUInt16();channels=r.ReadUInt16();rate=r.ReadInt32();r.ReadUInt32();r.ReadUInt16();bits=r.ReadUInt16();}
     else if(type=="data"){if(size>20000000)throw new InvalidDataException("Audio too large.");pcm=r.ReadBytes(size);}
     stream.Position=Math.Min(next,stream.Length);
    }
    if(format!=1||bits!=16||channels<1||channels>8||rate<8000||rate>96000||pcm==null||pcm.Length%(channels*2)!=0)throw new InvalidDataException("Expected original PCM16 audio.");
    var x=new double[pcm.Length/(channels*2)];for(int i=0;i<x.Length;i++){for(int c=0;c<channels;c++)x[i]+=BitConverter.ToInt16(pcm,(i*channels+c)*2);x[i]/=channels;}
    if(recipe.filter==2){if(rate!=44100||recipe.rate!=22050)throw new InvalidDataException("Unexpected pin audio.");var pair=new double[(x.Length-recipe.phase)/2];for(int i=0;i<pair.Length;i++)pair[i]=(x[2*i+recipe.phase]+x[2*i+recipe.phase+1])/2;return pair;}
    if(rate==recipe.rate)return x;
    int taps=recipe.filter,half=taps/2;if(taps!=63&&taps!=95)throw new InvalidDataException("Unknown audio filter.");double cutoff=.9*recipe.rate/rate,sum=0;var k=new double[taps];
    for(int j=0;j<taps;j++){int t=j-half;double sinc=t==0?1:Math.Sin(Math.PI*cutoff*t)/(Math.PI*cutoff*t);k[j]=cutoff*sinc*(.54-.46*Math.Cos(2*Math.PI*j/(taps-1)));sum+=k[j];}for(int j=0;j<taps;j++)k[j]/=sum;
    var f=new double[x.Length];for(int i=0;i<x.Length;i++)for(int j=0;j<taps;j++){int p=i+half-j;if(p>=0&&p<x.Length)f[i]+=x[p]*k[j];}
    int count=taps==63&&rate==44100?(x.Length+1)/2:(int)((long)x.Length*recipe.rate/rate);var y=new double[count];
    for(int i=0;i<count;i++){double p=(double)i*rate/recipe.rate;int lo=(int)p,hi=Math.Min(lo+1,f.Length-1);y[i]=f[lo]+(f[hi]-f[lo])*(p-lo);}return y;
   }
  }
  public static byte[] Cut(double[] x,AudioRecipe a){
   if(a.entry=="")return new byte[2];
   if(a.count<1||a.count>22050||a.start<0||a.start>x.Length-a.count)throw new InvalidDataException("Reload sound differs from supported source: "+a.weapon);
   var b=new byte[a.count*2];int fade=Math.Min(a.fade,a.count/2);
   for(int i=0;i<a.count;i++){double v=x[a.start+i];if(a.gainFirst)v*=a.gain;
    if(fade>1){if(i<fade)v*=i*(1.0/(fade-1));if(i>=a.count-fade)v*=1-(i-(a.count-fade))*(1.0/(fade-1));}
    if(!a.gainFirst)v*=a.gain;
    short n=(short)Math.Max(-32768,Math.Min(32767,a.truncate?Math.Truncate(v):Math.Round(v,MidpointRounding.ToEven)));b[i*2]=(byte)n;b[i*2+1]=(byte)(n>>8);
   }return b;
  }
  public static AudioRecipe[] Recipes(string json){
   var items=new JavaScriptSerializer{MaxJsonLength=4000000}.Deserialize<AudioRecipe[]>(json);
   if(items.Length!=822||items.Select(a=>a.id).Distinct().Count()!=822)throw new InvalidDataException("Audio manifest incomplete.");return items;
  }
  public static Dictionary<int,byte[]> Generate(string game,string recipes,Action<string> log){
   var items=Recipes(recipes);var result=new Dictionary<int,byte[]>();int fallback=0;
   var archives=new Dictionary<string,ZipArchive>(StringComparer.OrdinalIgnoreCase);var cache=new Dictionary<string,double[]>(StringComparer.OrdinalIgnoreCase);
   try{foreach(var a in items){byte[] cue;
    if(a.entry==""){result.Add(a.id,new byte[2]);continue;}
    string file=Path.GetFullPath(Path.Combine(game,a.archive.Replace('/',Path.DirectorySeparatorChar)));
    if(!file.StartsWith(Path.GetFullPath(game).TrimEnd('\\')+"\\mods\\",StringComparison.OrdinalIgnoreCase))throw new InvalidDataException("Invalid audio archive path.");
    try{
     ZipArchive z;if(!archives.TryGetValue(file,out z)){z=ZipFile.OpenRead(file);archives[file]=z;}
     string key=file+"|"+a.entry+"|"+a.filter+"|"+a.rate+"|"+a.phase;double[] x;
     if(!cache.TryGetValue(key,out x)){var e=z.Entries.FirstOrDefault(v=>v.FullName.Equals(a.entry,StringComparison.OrdinalIgnoreCase));if(e==null)throw new FileNotFoundException("Missing audio: "+a.entry);if(e.Length>20000000)throw new InvalidDataException("Unexpected sound size.");using(var m=new MemoryStream()){using(var s=e.Open())s.CopyTo(m);x=Decode(m.ToArray(),a);}cache[key]=x;}
     cue=Cut(x,a);
     if(PeImage.Hash(cue)!=a.sha256)throw new InvalidDataException("Sound differs from tested assets: "+a.weapon);
    }catch(Exception e){if(a.mod!="DZB2" || !(e is IOException||e is InvalidDataException))throw;cue=new byte[2];fallback++;}
    result.Add(a.id,cue);
   }}finally{foreach(var z in archives.Values)z.Dispose();}
   if(log!=null)log(fallback==0?"All 822 sounds match the tested build.":"Generated stock sounds; "+fallback+" optional DZB2 cues are silent until compatible DZB2 assets are installed. The DZB2 launcher refreshes them.");return result;
  }
  public static void Embed(string dll,Dictionary<int,byte[]> cues){
   if(cues.Count!=822)throw new InvalidDataException("Audio manifest incomplete.");byte[] original=File.ReadAllBytes(dll);IntPtr h=BeginUpdateResource(dll,false);if(h==IntPtr.Zero)throw Error();bool closed=false;
   try{foreach(var p in cues){if(p.Key<1100||p.Key>4001)throw new InvalidDataException("Invalid resource id.");if(!UpdateResource(h,(IntPtr)10,(IntPtr)p.Key,1033,p.Value,(uint)p.Value.Length))throw Error();}
    bool ok=EndUpdateResource(h,false);closed=true;if(!ok)throw Error();PreserveRelocationAddress(dll,original);
   }finally{if(!closed)EndUpdateResource(h,true);}
  }
  // Windows compacts .rsrc and may move the trailing relocation section. Keep
  // r134's mapped section addresses, including .reloc, even without optional
  // DZB2 audio. Raw file offsets may change; no code/data or fixup values do.
  static void PreserveRelocationAddress(string dll,byte[] original){
   byte[] current=File.ReadAllBytes(dll);int a=BitConverter.ToInt32(original,0x3c)+24,b=BitConverter.ToInt32(current,0x3c)+24;
   uint oldRva=BitConverter.ToUInt32(original,a+136),newRva=BitConverter.ToUInt32(current,b+136);
   int oldSections=a+BitConverter.ToUInt16(original,a-4);uint oldResourceSize=0;
   for(int i=0;i<BitConverter.ToUInt16(original,a-18);i++){int s=oldSections+40*i;if(System.Text.Encoding.ASCII.GetString(original,s,8).TrimEnd('\0')==".rsrc")oldResourceSize=BitConverter.ToUInt32(original,s+8);}
   int count=BitConverter.ToUInt16(current,b-18),sections=b+BitConverter.ToUInt16(current,b-4),reloc=-1;
   for(int i=0;i<count;i++){int s=sections+40*i;string name=System.Text.Encoding.ASCII.GetString(current,s,8).TrimEnd('\0');uint va=BitConverter.ToUInt32(current,s+12),size=BitConverter.ToUInt32(current,s+8);
    if(name==".rsrc"){if(oldResourceSize<size)throw new InvalidDataException("Resource image reservation exceeded.");Buffer.BlockCopy(BitConverter.GetBytes(oldResourceSize),0,current,s+8,4);}
    if(name==".reloc"){if(va!=newRva)throw new InvalidDataException("Unexpected relocation layout.");reloc=s;}
    else if(va>=oldRva||(ulong)va+size>oldRva)throw new InvalidDataException("Resource data exceeds reserved image space.");
   }
   if(reloc<0)throw new InvalidDataException("Missing relocation section.");
   Buffer.BlockCopy(BitConverter.GetBytes(oldRva),0,current,reloc+12,4);Buffer.BlockCopy(BitConverter.GetBytes(oldRva),0,current,b+136,4);
   Buffer.BlockCopy(original,a+56,current,b+56,4);Array.Clear(current,b+64,4);File.WriteAllBytes(dll,current);
  }
 }
}
