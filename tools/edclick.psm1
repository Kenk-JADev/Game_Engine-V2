# Test-Hilfsmittel: Klicks/Tasten an RuneEditor-Fenster (nur fuer Live-Tests).
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class EdClick {
  [StructLayout(LayoutKind.Sequential)]
  public struct RECT { public int L, T, R, B; }
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, ref RECT r);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, IntPtr e);
  [DllImport("user32.dll")] public static extern void keybd_event(byte k, byte s, uint f, IntPtr e);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowW(string cls, string title);
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h, int id);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
  [DllImport("user32.dll", EntryPoint="SendMessageW", CharSet=CharSet.Unicode)] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, string l);
  [DllImport("user32.dll", EntryPoint="SendMessageW", CharSet=CharSet.Unicode)] public static extern IntPtr SendMessageInt(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll", EntryPoint="SendMessageW", CharSet=CharSet.Unicode)] public static extern IntPtr SendMessageStr(IntPtr h, uint m, IntPtr w, System.Text.StringBuilder l);
  public static void Drag(int x1, int y1, int x2, int y2) {
    SetCursorPos(x1, y1); System.Threading.Thread.Sleep(200);
    mouse_event(2,0,0,0,IntPtr.Zero); System.Threading.Thread.Sleep(200);
    int steps = 12;
    for (int i = 1; i <= steps; i++) {
      int x = x1 + (x2 - x1) * i / steps;
      int y = y1 + (y2 - y1) * i / steps;
      SetCursorPos(x, y);
      mouse_event(1,0,0,0,IntPtr.Zero);
      System.Threading.Thread.Sleep(60);
    }
    System.Threading.Thread.Sleep(200);
    mouse_event(4,0,0,0,IntPtr.Zero); System.Threading.Thread.Sleep(400);
  }
  public static void Click(int x, int y) {
    SetCursorPos(x, y); System.Threading.Thread.Sleep(150);
    mouse_event(2,0,0,0,IntPtr.Zero); System.Threading.Thread.Sleep(80);
    mouse_event(4,0,0,0,IntPtr.Zero); System.Threading.Thread.Sleep(400);
  }
  public static void Key(byte vk) {
    keybd_event(vk, 0, 0, IntPtr.Zero); System.Threading.Thread.Sleep(40);
    keybd_event(vk, 0, 2, IntPtr.Zero); System.Threading.Thread.Sleep(80);
  }
  public static void CtrlKey(byte vk) {
    keybd_event(0x11, 0, 0, IntPtr.Zero); System.Threading.Thread.Sleep(60);
    keybd_event(vk, 0, 0, IntPtr.Zero); System.Threading.Thread.Sleep(50);
    keybd_event(vk, 0, 2, IntPtr.Zero); System.Threading.Thread.Sleep(60);
    keybd_event(0x11, 0, 2, IntPtr.Zero); System.Threading.Thread.Sleep(80);
  }
  public static void SetDlgText(int id, string s) {
    IntPtr dlg = FindWindowW("RuneEditorDlg", null);
    if (dlg == IntPtr.Zero) dlg = FindWindowW("RuneEditorDlgNew", null);
    if (dlg == IntPtr.Zero) { Console.WriteLine("kein Dialog"); return; }
    IntPtr ed = GetDlgItem(dlg, id);
    if (ed == IntPtr.Zero) { Console.WriteLine("kein Edit " + id); return; }
    SendMessage(ed, 0x000C, IntPtr.Zero, s);
  }
}
"@
Add-Type -AssemblyName System.Drawing
function Get-EdRect {
    $h = (Get-Process RuneEditor).MainWindowHandle
    $r = New-Object EdClick+RECT
    [EdClick]::GetWindowRect($h, [ref]$r) | Out-Null
    return $r
}
function Save-Shot([string]$path) {
    $b = New-Object System.Drawing.Bitmap(1500, 950)
    $g = [System.Drawing.Graphics]::FromImage($b)
    $g.CopyFromScreen(0, 0, 0, 0, $b.Size)
    $b.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $b.Dispose()
}
# Objektliste des Editors auslesen (IDC_OBJ_LIST = 3010)
function Get-ObjList {
    $h = (Get-Process RuneEditor).MainWindowHandle
    $lb = [EdClick]::GetDlgItem($h, 3010)
    $n = [int][EdClick]::SendMessageInt($lb, 0x018B, [IntPtr]::Zero, [IntPtr]::Zero)
    $out = @()
    for ($i = 0; $i -lt $n; $i++) {
        $len = [int][EdClick]::SendMessageInt($lb, 0x018A, [IntPtr]$i, [IntPtr]::Zero)
        $sb = New-Object System.Text.StringBuilder ($len + 2)
        [EdClick]::SendMessageStr($lb, 0x0189, [IntPtr]$i, $sb) | Out-Null
        $out += $sb.ToString()
    }
    return $out
}
function Get-PropField([int]$id) {
    $h = (Get-Process RuneEditor).MainWindowHandle
    $ed = [EdClick]::GetDlgItem($h, $id)
    $sb = New-Object System.Text.StringBuilder 64
    [EdClick]::SendMessageStr($ed, 0x000D, [IntPtr]64, $sb) | Out-Null   # WM_GETTEXT
    return $sb.ToString()
}

# Fenster in den Vordergrund holen und Klick in Client-Koordinaten ausloesen
function Focus-Ed {
    $h = (Get-Process RuneEditor).MainWindowHandle
    [EdClick]::SetForegroundWindow($h) | Out-Null
    Start-Sleep -Milliseconds 300
}
function Get-ScreenPt([int]$cx, [int]$cy) {
    $h = (Get-Process RuneEditor).MainWindowHandle
    $p = New-Object EdClick+POINT
    $p.X = $cx; $p.Y = $cy
    [EdClick]::ClientToScreen($h, [ref]$p) | Out-Null
    return $p
}
function Click-Client([int]$cx, [int]$cy) {
    $p = Get-ScreenPt $cx $cy
    [EdClick]::Click($p.X, $p.Y)
}

# Direkte Mausnachrichten an das Viewport-Child (unabhaengig vom Fokus)
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class EdPost {
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowExW(IntPtr parent, IntPtr after, string cls, string title);
  [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);
  public static IntPtr View() {
    IntPtr main = (IntPtr)0;
    foreach (var p in System.Diagnostics.Process.GetProcessesByName("RuneEditor")) {
      if (p.MainWindowHandle != IntPtr.Zero) { main = p.MainWindowHandle; break; }
    }
    return FindWindowExW(main, IntPtr.Zero, "RuneEditorView", null);
  }
  public static void Post(int x, int y, uint msg, int flags) {
    IntPtr l = (IntPtr)((y << 16) | (x & 0xFFFF));
    PostMessageW(View(), msg, (IntPtr)flags, l);
  }
  public static void ClickV(int x, int y) {
    Post(x, y, 0x0201, 1); System.Threading.Thread.Sleep(120);
    Post(x, y, 0x0202, 0); System.Threading.Thread.Sleep(250);
  }
  public static void DragV(int x1, int y1, int x2, int y2) {
    Post(x1, y1, 0x0201, 1); System.Threading.Thread.Sleep(150);
    int steps = 10;
    for (int i = 1; i <= steps; i++) {
      int x = x1 + (x2 - x1) * i / steps;
      int y = y1 + (y2 - y1) * i / steps;
      Post(x, y, 0x0200, 1);
      System.Threading.Thread.Sleep(70);
    }
    Post(x2, y2, 0x0200, 1); System.Threading.Thread.Sleep(120);
    Post(x2, y2, 0x0202, 0); System.Threading.Thread.Sleep(300);
  }
}
"@

# Statusleisten-Teil lesen (SB_GETTEXTW = WM_USER+13)
function Get-StatusText([int]$part) {
    $h = (Get-Process RuneEditor).MainWindowHandle
    $sb = [EdClick]::GetDlgItem($h, 3016)
    $buf = New-Object System.Text.StringBuilder 256
    [EdClick]::SendMessageStr($sb, 0x040D, [IntPtr]$part, $buf) | Out-Null   # SB_GETTEXTW = WM_USER+13 (A-Variante waere WM_USER+2)
    return $buf.ToString()
}
