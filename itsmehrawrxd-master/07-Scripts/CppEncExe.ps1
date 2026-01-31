# RawrZ CppEncExe.ps1 - Public Domain C++ Compiler with Camellia Encryption
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
# Drag-and-drop C++ compilation with Camellia encryption
# Drag any .cpp file onto it → compiles in memory, encrypts with Camellia-256, emits cipher-text to stdout
$args|%{$c=[IO.File]::ReadAllText($_);Add-Type -TypeDefinition 'using System;using System.IO;using System.Security.Cryptography;public class C{public static byte[]E(byte[]p,byte[]k,byte[]i){var m=new MemoryStream();var c=new CamelliaManaged(){Key=k,IV=i};var t=c.CreateEncryptor();var s=new CryptoStream(m,t,CryptoStreamMode.Write);s.Write(p,0,p.Length);s.Close();return m.ToArray();}}';$k=RandomNumberGenerator.GetBytes(32);$i=RandomNumberGenerator.GetBytes(16);$o=Join-Path ([IO.Path]::GetTempPath()) "$([IO.Path]::GetFileNameWithoutExtension($_)).exe";$p=Start-Process -FilePath "clang++" -ArgumentList @("-x","c++","-o",$o,"-O2","-static","-"),InputString $c,NoNewWindow:$true,Wait:$true,PassThru:$true;$e=[IO.File]::ReadAllBytes($o);$x=C::E($e,$k,$i);[Console]::OpenStandardOutput().Write($x,0,$x.Length);[Console]::Error.WriteLine("Key="+[Convert]::ToBase64String($k)+" IV="+[Convert]::ToBase64String($i));Write-Host "✔ Compiled and encrypted: $_" -ForegroundColor Cyan}
