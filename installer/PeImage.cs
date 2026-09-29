using System;
using System.IO;
using System.Linq;
using System.Text;
using System.Collections.Generic;
using System.Security.Cryptography;

namespace BF2VR {
 public sealed class PeImage {
  readonly byte[] b; readonly int pe,opt,sections,count;
  public PeImage(byte[] data) {
   b=data;
   if(b.Length<256 || U16(0)!=0x5a4d) throw new InvalidDataException("Not a Windows executable.");
   pe=checked((int)U32(0x3c));
   if(pe<64 || pe>b.Length-256 || U32(pe)!=0x4550 || U16(pe+4)!=0x14c) throw new InvalidDataException("BF2-VR requires a 32-bit x86 executable.");
   opt=pe+24; count=U16(pe+6); sections=opt+U16(pe+20);
   if(U16(opt)!=0x10b || count<1 || count>32 || sections+count*40>b.Length || U32(opt+92)<16) throw new InvalidDataException("Unsupported PE layout.");
  }
  ushort U16(int p){return BitConverter.ToUInt16(b,p);} uint U32(int p){return BitConverter.ToUInt32(b,p);}
  static void Put(byte[] v,int p,uint n){Buffer.BlockCopy(BitConverter.GetBytes(n),0,v,p,4);}
  static int Align(int n,int a){if(a<1 || (a&(a-1))!=0)throw new InvalidDataException("Invalid alignment.");return checked((n+a-1)&~(a-1));}
  string Name(int s){return Encoding.ASCII.GetString(b,s,8).TrimEnd('\0');}
  int Offset(uint r){for(int i=0;i<count;i++){int s=sections+i*40;uint va=U32(s+12),raw=U32(s+16);if(r>=va && r-va<raw){long p=(long)U32(s+20)+r-va;if(p>=b.Length)break;return (int)p;}}throw new InvalidDataException("Invalid PE address.");}
  public static string Hash(byte[] v){using(var h=SHA256.Create())return BitConverter.ToString(h.ComputeHash(v)).Replace("-","").ToLowerInvariant();}
  public string TextHash(){for(int i=0;i<count;i++){int s=sections+i*40;if(Name(s)==".text"){int n=checked((int)U32(s+8)),p=checked((int)U32(s+20));if(n<1 || n>U32(s+16) || p<0 || p>b.Length-n)throw new InvalidDataException("Invalid code section.");return Hash(b.Skip(p).Take(n).ToArray());}}throw new InvalidDataException("Missing code section.");}
  List<int> Imports(){var list=new List<int>(); int p=Offset(U32(opt+104));for(int i=0;i<256;i++,p+=20){if(p>b.Length-20)break;if(U32(p)==0 && U32(p+12)==0 && U32(p+16)==0)return list;list.Add(p);}throw new InvalidDataException("Invalid imports.");}
  string Z(uint r){int p=Offset(r),end=p;while(end<b.Length && b[end]!=0 && end-p<256)end++;if(end==b.Length || end-p==256)throw new InvalidDataException("Invalid import name.");return Encoding.ASCII.GetString(b,p,end-p);}
  public bool HasRfx(){return Imports().Any(p=>String.Equals(Z(U32(p+12)),"RFX.dll",StringComparison.OrdinalIgnoreCase));}
  public byte[] AddRfxImport(){
   if(HasRfx())return (byte[])b.Clone();
   var imports=Imports();int sa=checked((int)U32(opt+32)),fa=checked((int)U32(opt+36));
   int firstRaw=Int32.MaxValue,maxVA=0;
   for(int i=0;i<count;i++){int s=sections+i*40;int raw=checked((int)U32(s+20));if(raw>0)firstRaw=Math.Min(firstRaw,raw);maxVA=Math.Max(maxVA,checked((int)U32(s+12)+(int)Math.Max(U32(s+8),U32(s+16))));}
   int sh=sections+count*40;
   if(sh+40>firstRaw || sh+40>U32(opt+60))throw new InvalidDataException("No spare executable section header; original was not changed.");
   int rva=Align(maxVA,sa),offset=Align(b.Length,fa),table=(imports.Count+2)*20;
   int ilt=table,iat=ilt+8,dll=iat+8,hint=dll+8,len=hint+8;
   byte[] result=new byte[checked(offset+Align(len,fa))];Buffer.BlockCopy(b,0,result,0,b.Length);
   for(int i=0;i<imports.Count;i++){Buffer.BlockCopy(b,imports[i],result,offset+i*20,20);Put(result,offset+i*20+4,0);Put(result,offset+i*20+8,0);}
   int d=offset+imports.Count*20;Put(result,d,(uint)(rva+ilt));Put(result,d+12,(uint)(rva+dll));Put(result,d+16,(uint)(rva+iat));
   Put(result,offset+ilt,(uint)(rva+hint));Put(result,offset+iat,(uint)(rva+hint));
   Buffer.BlockCopy(Encoding.ASCII.GetBytes("RFX.dll\0"),0,result,offset+dll,8);
   Buffer.BlockCopy(Encoding.ASCII.GetBytes("main\0"),0,result,offset+hint+2,5);
   Array.Clear(result,sh,40);Buffer.BlockCopy(Encoding.ASCII.GetBytes(".vrimp"),0,result,sh,6);
   Put(result,sh+8,(uint)len);Put(result,sh+12,(uint)rva);Put(result,sh+16,(uint)Align(len,fa));Put(result,sh+20,(uint)offset);Put(result,sh+36,0xc0000040);
   Buffer.BlockCopy(BitConverter.GetBytes((ushort)(count+1)),0,result,pe+6,2);
   Put(result,opt+8,checked(U32(opt+8)+(uint)Align(len,fa)));
   Put(result,opt+56,(uint)Align(rva+len,sa));Put(result,opt+64,0);
   Put(result,opt+104,(uint)rva);Put(result,opt+108,(uint)table);
   // Bound imports and certificate offsets no longer describe this local copy.
   Put(result,opt+96+11*8,0);Put(result,opt+100+11*8,0);Put(result,opt+96+4*8,0);Put(result,opt+100+4*8,0);
   var check=new PeImage(result);if(!check.HasRfx() || check.TextHash()!=TextHash())throw new InvalidDataException("Import update verification failed.");return result;
  }
  public static void ValidateGame(string folder){
   var exe=new PeImage(File.ReadAllBytes(Path.Combine(folder,"BF2.exe")));
   var renderer=new PeImage(File.ReadAllBytes(Path.Combine(folder,"RendDX9.dll")));
   if(exe.TextHash()!="b87ccb0fd5adf3a4bcff2c6562bba412babc964795e5d5b74f62f5c353fb3c3e" || renderer.TextHash()!="13f8dd6a473f7a566b06434c685be7b2b79427918cf7c55315cd91a0a8e418d9")
    throw new InvalidDataException("This BF2.exe / RendDX9.dll combination is not the supported BF2 1.5 build. First patch to 1.5 and run the game flat-screen. This installer does not download or replace game executables. Please report an unsupported legitimate 1.5 build on GitHub.");
   if(!File.Exists(Path.Combine(folder,"mods","bf2","Objects_client.zip")))throw new FileNotFoundException("Select the Battlefield 2 folder containing BF2.exe and mods/bf2/Objects_client.zip.");
  }
 }
}
