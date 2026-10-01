// Reference excerpts only: these blocks belong to different existing scopes.
// Do not compile this file as a standalone translation unit.

// Before normal WinForms startup in Program.Main(string[] args).
// Origin: tools/VrSettingsApp.cs, source line 15
        if(args.Length==3&&args[0]=="--resolve-openxr"){
            try{File.WriteAllText(args[2],VrResolutionPlan.Resolve(args[1]),new UTF8Encoding(false));Environment.ExitCode=0;}
            catch(Exception e){File.WriteAllText(args[2],e.Message,new UTF8Encoding(false));Environment.ExitCode=1;}return;
        }
        if(args.Length==3&&args[0]=="--probe-openxr"){
            try{File.WriteAllText(args[2],VrResolution.Probe(args[1]));Environment.ExitCode=0;}
            catch(Exception e){File.WriteAllText(args[2],e.Message);Environment.ExitCode=1;}return;
        }
