// Outside-process, non-activating GDI companion. No game hooks or injection.
using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Web.Script.Serialization;
using System.Windows.Forms;

class LiveHud : Form {
    [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc proc,IntPtr param);
    delegate bool EnumProc(IntPtr window,IntPtr param);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr window,out uint pid);
    [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] static extern bool IsIconic(IntPtr window);
    [DllImport("user32.dll")] static extern bool GetClientRect(IntPtr window,out Rect rect);
    [DllImport("user32.dll")] static extern bool ClientToScreen(IntPtr window,ref Point p);
    [DllImport("user32.dll")] static extern uint GetDpiForWindow(IntPtr window);
    [DllImport("user32.dll")] static extern bool SetProcessDpiAwarenessContext(IntPtr context);
    [DllImport("user32.dll")] static extern bool SetWindowPos(IntPtr h,IntPtr after,int x,int y,int w,int height,uint flags);
    [DllImport("user32.dll",SetLastError=true)] static extern bool RegisterHotKey(IntPtr h,int id,uint mods,uint key);
    [DllImport("user32.dll")] static extern bool UnregisterHotKey(IntPtr h,int id);
    [DllImport("user32.dll")] static extern IntPtr GetForegroundWindow();
    [StructLayout(LayoutKind.Sequential)] struct Rect { public int left,top,right,bottom; }
    string root,session,settings; int pid; bool expanded=false,shown=true; int marginX=28,marginY=28,widthDip=370;
    Dictionary<string,object> data=new Dictionary<string,object>();
    JavaScriptSerializer json=new JavaScriptSerializer(){MaxJsonLength=8*1024*1024};
    NotifyIcon tray; Form panel; System.Windows.Forms.Timer timer; string error="Waiting for sensors",state="STARTING";
    double age; DateTime lastRead=DateTime.MinValue; IntPtr game; float scale=1; int refreshes=0;
    protected override bool ShowWithoutActivation {get{return true;}}
    bool hotkeyControls,hotkeyHide,hotkeyMarker;int hotkeyMessages;
    protected override void OnHandleCreated(EventArgs e){base.OnHandleCreated(e);hotkeyControls=RegisterHotKey(Handle,1,0x4000|2|4,0x24);hotkeyHide=RegisterHotKey(Handle,2,0x4000|2|4,0x2D);hotkeyMarker=RegisterHotKey(Handle,3,0x4000|2|4,0x21);}
    protected override CreateParams CreateParams {get{var p=base.CreateParams;p.ExStyle|=0x08000000|0x20|0x80;p.ExStyle|=0x80000;return p;}}
    protected override void WndProc(ref Message m){
        if(m.Msg==0x84){m.Result=new IntPtr(-1);return;}
        if(m.Msg==0x21){m.Result=new IntPtr(3);return;}
        if(m.Msg==0x312){hotkeyMessages++;if(m.WParam.ToInt32()==1)ControlPanel();if(m.WParam.ToInt32()==2){shown=!shown;Tick();}if(m.WParam.ToInt32()==3)Command("marker","performance issue");}
        base.WndProc(ref m);
    }
    static Dictionary<string,object> Map(object o){return o as Dictionary<string,object>??new Dictionary<string,object>();}
    static object At(Dictionary<string,object>d,string k){object o;return d.TryGetValue(k,out o)?o:null;}
    static double? Num(object o){if(o==null)return null;double n;return double.TryParse(Convert.ToString(o,System.Globalization.CultureInfo.InvariantCulture),System.Globalization.NumberStyles.Float,System.Globalization.CultureInfo.InvariantCulture,out n)?(double?)n:null;}
    static string F(object o,string format="0.0"){var n=Num(o);return n.HasValue?n.Value.ToString(format):"N/A";}
    string Rate(Dictionary<string,object> f,string key){var m=Map(At(f,key));return Convert.ToString(At(m,"status"))=="MEASURED"?F(At(m,"value")):Convert.ToString(At(m,"status"))=="STALE"?"STALE":"NOT MEASURED";}
    Dictionary<string,object> Read(string path){
        using(var f=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.ReadWrite|FileShare.Delete))
        using(var r=new StreamReader(f)){return Map(json.DeserializeObject(r.ReadToEnd()));}
    }
    public LiveHud(string path,int target){
        root=path;pid=target;settings=Path.Combine(root,"hud-settings.json");Directory.CreateDirectory(Path.Combine(root,"commands"));
        Text="Vulkan Research Monitor";FormBorderStyle=FormBorderStyle.None;ShowInTaskbar=false;TopMost=true;BackColor=Color.FromArgb(15,20,29);ForeColor=Color.White;Opacity=.87;DoubleBuffered=true;
        tray=new NotifyIcon(){Icon=SystemIcons.Information,Text="Vulkan Research Monitor",Visible=true};
        var menu=new ContextMenuStrip();menu.Items.Add("Controls / sensors (Ctrl+Shift+Home)",null,(s,e)=>ControlPanel());
        menu.Items.Add("Show / hide HUD (Ctrl+Shift+Insert)",null,(s,e)=>{shown=!shown;Tick();});
        menu.Items.Add("Mark performance issue (Ctrl+Shift+PageUp)",null,(s,e)=>Command("marker","performance issue"));
        menu.Items.Add("Expanded statistics",null,(s,e)=>{expanded=!expanded;Tick();});
        tray.ContextMenuStrip=menu;tray.DoubleClick+=(s,e)=>ControlPanel();
        FormClosed+=(s,e)=>{for(int i=1;i<=3;i++)UnregisterHotKey(Handle,i);tray.Dispose();if(panel!=null)panel.Close();};
        timer=new System.Windows.Forms.Timer(){Interval=1000};timer.Tick+=(s,e)=>Tick();timer.Start();Tick();
    }
    void Command(string kind,string label=""){
        var c=new {command=kind,label=label,session=session,pid=pid,utc=DateTime.UtcNow.ToString("o"),qpc_s=(double)Stopwatch.GetTimestamp()/Stopwatch.Frequency};
        string name=Path.Combine(root,"commands",Guid.NewGuid().ToString("N"));File.WriteAllText(name+".tmp",json.Serialize(c));File.Move(name+".tmp",name+".json");
    }
    void ControlPanel(){
        if(panel!=null&&!panel.IsDisposed){panel.Show();panel.Activate();return;}
        panel=new Form(){Text="Vulkan Research Monitor — controls",Size=new Size(540,540),TopMost=true,StartPosition=FormStartPosition.CenterScreen,AutoScaleMode=AutoScaleMode.None,Font=new Font("Segoe UI",13,FontStyle.Regular,GraphicsUnit.Pixel)};
        var flow=new FlowLayoutPanel(){Dock=DockStyle.Fill,AutoScroll=true,Padding=new Padding(12)};panel.Controls.Add(flow);
        Action<string,string,string> button=(title,command,label)=>{var b=new Button(){Text=title,Size=new Size(150,34)};b.Click+=(s,e)=>Command(command,label);flow.Controls.Add(b);};
        button("Start recording","start","");button("Stop recording","stop","");button("Pause / resume","pause","");
        foreach(var label in new[]{"Gameplay","Loading","Combat","New location","Performance issue","Graphics effect","Crash marker"})button("Mark "+label,"marker",label);
        button("Anomaly snapshot","snapshot","");button("Export report","export","");
        var folder=new Button(){Text="Recording folder",Size=new Size(150,34)};folder.Click+=(s,e)=>{if(session!=null)Process.Start("explorer.exe",session);};flow.Controls.Add(folder);
        var more=new Button(){Text="Expand HUD",Size=new Size(150,34)};more.Click+=(s,e)=>{expanded=!expanded;Tick();};flow.Controls.Add(more);
        foreach(var pair in new[]{new[]{"Right margin",marginX.ToString()},new[]{"Top margin",marginY.ToString()},new[]{"Width",widthDip.ToString()}}){
            flow.Controls.Add(new Label(){Text=pair[0],Width=140,Height=26});var input=new NumericUpDown(){Minimum=0,Maximum=2000,Value=int.Parse(pair[1]),Width=80};flow.Controls.Add(input);
            input.ValueChanged+=(s,e)=>{if(pair[0]=="Right margin")marginX=(int)input.Value;else if(pair[0]=="Top margin")marginY=(int)input.Value;else widthDip=Math.Max(300,(int)input.Value);File.WriteAllText(settings,json.Serialize(new{right=marginX,top=marginY,width=widthDip}));Tick();};
        }
        var health=new TextBox(){Multiline=true,ReadOnly=true,ScrollBars=ScrollBars.Vertical,Width=490,Height=140};flow.Controls.Add(health);
        var t=new System.Windows.Forms.Timer(){Interval=1000};t.Tick+=(s,e)=>health.Text="State: "+state+"\r\n"+error+"\r\nSession: "+session+"\r\nCPU stacks: unavailable (non-elevated)\r\nGuest frame IDs / GPU stage time: require matching instrumentation\r\nETW losses: unknown; no stream loss counter";t.Start();panel.FormClosed+=(s,e)=>t.Dispose();panel.Show();panel.Activate();
    }
    void Tick(){
        try{
            string request=Path.Combine(root,"SHOW_CONTROLS");if(File.Exists(request)){File.Delete(request);ControlPanel();}
            var active=Read(Path.Combine(root,"active-session.json"));session=Convert.ToString(At(active,"session"));
            if(File.Exists(Path.Combine(root,"hud-exit-"+pid))){Close();return;}
            if(File.Exists(settings)){var p=Read(settings);marginX=(int)(Num(At(p,"right"))??28);marginY=(int)(Num(At(p,"top"))??28);widthDip=Math.Max(300,(int)(Num(At(p,"width"))??370));}
            var status=Read(Path.Combine(session,"status.json"));state=Convert.ToString(At(status,"status"));
            if(File.Exists(Path.Combine(session,"live.json"))){data=Read(Path.Combine(session,"live.json"));lastRead=DateTime.Parse(Convert.ToString(At(data,"utc"))).ToUniversalTime();}
            age=(DateTime.UtcNow-lastRead).TotalSeconds;
            error=state=="paused"?"Recording paused — values held":age>5?"STALE sensors — "+age.ToString("0")+"s":Convert.ToString(At(Map(At(data,"frames")),"presentmon_error"));
            game=IntPtr.Zero;int best=0;
            EnumWindows((w,p)=>{uint id;GetWindowThreadProcessId(w,out id);Rect r;if(id==pid&&IsWindowVisible(w)&&!IsIconic(w)&&GetClientRect(w,out r)){int area=(r.right-r.left)*(r.bottom-r.top);if(area>best){best=area;game=w;}}return true;},IntPtr.Zero);
            uint foregroundPid;GetWindowThreadProcessId(GetForegroundWindow(),out foregroundPid);
            if(game!=IntPtr.Zero&&shown&&(foregroundPid==pid||foregroundPid==Process.GetCurrentProcess().Id)){
                Rect r;GetClientRect(game,out r);Point origin=Point.Empty;ClientToScreen(game,ref origin);scale=GetDpiForWindow(game)/96f;if(scale<=0)scale=1;
                int w=(int)(widthDip*scale),h=(int)((expanded?700:430)*scale);
                SetWindowPos(Handle,new IntPtr(-1),origin.X+Math.Max(0,r.right-w-(int)(marginX*scale)),origin.Y+(int)(marginY*scale),w,h,0x10|0x40);if(!Visible)Show();
            }else Hide();
            refreshes++;Invalidate();
            File.WriteAllText(Path.Combine(root,"hud-health.tmp"),json.Serialize(new{pid=Process.GetCurrentProcess().Id,target_pid=pid,utc=DateTime.UtcNow.ToString("o"),refreshes=refreshes,session=session,visible=Visible,game_window=game.ToInt64(),foreground=GetForegroundWindow().ToInt64(),sensor_age_s=age,expanded=expanded,click_through=true,no_activate=true,hotkey_controls=hotkeyControls,hotkey_hide=hotkeyHide,hotkey_marker=hotkeyMarker,hotkey_messages=hotkeyMessages,cpu_total_s=Process.GetCurrentProcess().TotalProcessorTime.TotalSeconds,ram_bytes=Process.GetCurrentProcess().WorkingSet64}));
            string hp=Path.Combine(root,"hud-health.json");if(File.Exists(hp))File.Replace(Path.Combine(root,"hud-health.tmp"),hp,null);else File.Move(Path.Combine(root,"hud-health.tmp"),hp);
        }catch(Exception e){error=e.Message;state="ERROR";Invalidate();}
    }
    protected override void OnPaint(PaintEventArgs e){
        base.OnPaint(e);e.Graphics.PageUnit=GraphicsUnit.Pixel;e.Graphics.ScaleTransform(scale,scale);using(var font=new Font("Consolas",13,FontStyle.Regular,GraphicsUnit.Pixel))using(var title=new Font("Segoe UI",15,FontStyle.Bold,GraphicsUnit.Pixel)){
            float y=14;int right=widthDip-14;Action<string,string,Color> row=(name,value,color)=>{e.Graphics.DrawString(name,font,Brushes.White,14,y);using(var brush=new SolidBrush(color)){var size=e.Graphics.MeasureString(value,font);e.Graphics.DrawString(value,font,brush,Math.Max(175,right-size.Width),y);}y+=22;};
            e.Graphics.DrawString("VULKAN RESEARCH",title,Brushes.White,14,y);e.Graphics.DrawString(state=="recording"&&age<5?"● REC":state.ToUpperInvariant(),title,state=="recording"&&age<5?Brushes.LimeGreen:state=="ERROR"?Brushes.Red:Brushes.Gold,245,y);y+=32;
            row("SESSION / PID",TimeSpan.FromSeconds(Num(At(data,"elapsed_s"))??0).ToString(@"hh\:mm\:ss")+" / "+pid,Color.White);
            row("BUILD",Convert.ToString(At(data,"build_label")),Color.White);
            var f=Map(At(data,"frames"));row("BASE GAME FPS",Rate(f,"guest_fps"),Color.Gold);row("HOST PRESENT FPS",Rate(f,"host_fps"),Color.Gold);row("DISPLAY FPS",Rate(f,"display_fps"),Color.Gold);
            row("APP PRESENT/SHOWN",Rate(f,"integration_present_fps").Replace("NOT MEASURED","N/A")+" / "+Rate(f,"integration_shown_fps").Replace("NOT MEASURED","N/A"),Color.Gold);
            row("GUEST FRAME MS",Rate(f,"frame_time_ms"),Color.Gold);
            string fg=Convert.ToString(At(f,"fg_mode"));row("FG",fg.Contains("generation off")?"x4 requested / log OFF":fg.Contains("generation enabled")?"x4 requested / log ON":"x4 requested / unverified",Color.Gold);y+=6;
            var gpu=Map(At(data,"selected_gpu"));double? three=null,mem=null;
            var engines=At(gpu,"engines") as object[]??new object[0];foreach(var obj in engines){var g=Map(obj);if(Convert.ToString(At(g,"instance")).Contains("engtype_3D")&&(Num(At(g,"status"))==0||Num(At(g,"status"))==1)){double? n=Num(At(g,"value"));if(n.HasValue)three=Math.Max(three??0,n.Value);}}
            var memories=At(gpu,"dedicated") as object[]??new object[0];foreach(var obj in memories){var g=Map(obj);if(Num(At(g,"status"))==0||Num(At(g,"status"))==1)mem=(mem??0)+(Num(At(g,"value"))??0);}
            row("GPU 3D / VRAM",F(three)+"% / "+F(mem.HasValue?(object)(mem.Value/1073741824):null)+" GiB",Color.White);
            row("CPU / RAM",F(At(data,"cpu_core_equivalents"))+" cores / "+F((Num(At(data,"working_set_bytes"))??0)/1073741824)+" GiB",Color.White);
            row("SAMPLES / MISSED",F(At(data,"sample_count"),"0")+" / "+F(At(data,"skipped_intervals"),"0"),Color.White);
            var coverage=Map(At(data,"coverage"));row("COVERAGE",F(At(coverage,"available"),"0")+"/"+F(At(coverage,"total"),"0")+" metrics",Color.Gold);
            row("ETW LOSS / DEEP","UNKNOWN / OFF",Color.Gold);
            if(expanded){
                foreach(var kind in new[]{"Copy","Compute"}){
                    double? utilization=null;
                    foreach(var obj in engines){var g=Map(obj);if(Convert.ToString(At(g,"instance")).Contains("engtype_"+kind)&&(Num(At(g,"status"))==0||Num(At(g,"status"))==1))utilization=Math.Max(utilization??0,Num(At(g,"value"))??0);}
                    row("GPU "+kind.ToUpperInvariant(),F(utilization)+"% (max engine)",Color.White);
                }
                row("MEMORY PRESSURE",F(At(Map(At(data,"system_memory")),"load"))+"%",Color.White);
                row("PAGE FAULTS",F(At(data,"page_faults"),"0")+" (soft included)",Color.White);
                row("IO READ / WRITE",F((Num(At(Map(At(data,"io")),"read_bytes"))??0)/1048576)+" / "+F((Num(At(Map(At(data,"io")),"write_bytes"))??0)/1048576)+" MiB",Color.White);
                var ts=(At(data,"threads") as object[]??new object[0]).Select(Map).OrderByDescending(t=>Num(At(t,"cpu_core_equivalents"))??-1).Take(4);
                foreach(var t in ts){string n=Convert.ToString(At(t,"name"));row(n.Length>20?n.Substring(0,20):n,F(At(t,"cpu_core_equivalents"))+" cores",Color.White);}
                row("COLLECT WALL",F((Num(At(data,"collection_wall_s"))??0)*1000)+" ms",Color.White);
                row("PRESENT ROWS",F(At(f,"presentmon_rows"),"0"),Color.White);
                row("LAST EVENT AGE",F(age)+" s",Color.White);
            }
            var last=Map(At(data,"last_anomaly"));var candidates=At(last,"labels") as object[]??At(data,"anomaly_candidates") as object[]??new object[0];string anomaly=candidates.Length>0?Convert.ToString(candidates[0]):"none observed";
            using(var small=new Font("Segoe UI",11,FontStyle.Regular,GraphicsUnit.Pixel)){
                e.Graphics.DrawString("Anomaly: "+anomaly,small,Brushes.Gold,14,y);y+=20;
                e.Graphics.DrawString(string.IsNullOrEmpty(error)?"Recording / partial frame metrics":error,small,age>5&&state!="paused"?Brushes.Red:Brushes.Gold,new RectangleF(14,y,widthDip-28,42));
            }
        }
    }
    [STAThread] static void Main(string[]args){try{SetProcessDpiAwarenessContext(new IntPtr(-4));}catch{}Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);Application.Run(new LiveHud(args[0],int.Parse(args[1])));}
}
