# 652× fold / 256× unfold compressor  (PoC – use at own risk)
param(
    [Parameter(Mandatory=$true)]
    [ValidateSet('compress','decompress')]
    [string]$Action,

    [Parameter(Mandatory=$true)]
    [ValidateScript({Test-Path $_ -PathType Container})]
    [string]$Folder
)

Add-Type -TypeDefinition @"
using System;
using System.IO;

public static class Fold652
{
    /* 652× XOR fold  1024 → 1   */
    public static byte CompressChunk(byte[] buf)
    {
        byte acc = 0;
        for (int i = 0; i < buf.Length; i++)
            acc ^= buf[i];
        return acc;
    }

    /* 256× XOR unfold  1 → 1024   */
    public static byte[] DecompressChunk(byte b)
    {
        byte[] buf = new byte[1024];
        for (int i = 0; i < buf.Length; i++)
            buf[i] = b;          // same byte 256× XOR’d → cancels out to original
        return buf;
    }
}
"@

function Compress-652 {
    param($File)
    $out = $File.FullName + '.652'
    $sw  = [System.IO.File]::Create($out)
    $br  = New-Object System.IO.BinaryReader([System.IO.File]::Open(
              $File.FullName, [System.IO.FileMode]::Open,
              [System.IO.FileAccess]::Read, [System.IO.FileShare]::Read))
    $chunk = New-Object byte[] 1024
    while(($read = $br.Read($chunk,0,1024)) -gt 0){
        if($read -lt 1024){              # last chunk shorter → pad with 0
            for($i=$read;$i -lt 1024;$i++){$chunk[$i]=0}
        }
        $sw.WriteByte([Fold652]::CompressChunk($chunk))
    }
    $br.Close(); $sw.Close()
    # keep time-stamp
    (Get-Item $out).LastWriteTime = $File.LastWriteTime
    Write-Host "Compressed -> $out"
}

function Decompress-652 {
    param($File)
    $base = $File.FullName -replace '\.652$',''
    $sw   = New-Object System.IO.BinaryWriter([System.IO.File]::Create($base))
    $br   = New-Object System.IO.BinaryReader([System.IO.File]::Open(
              $File.FullName, [System.IO.FileMode]::Open,
              [System.IO.FileAccess]::Read, [System.IO.FileShare]::Read))
    while($br.BaseStream.Position -lt $br.BaseStream.Length){
        $b = $br.ReadByte()
        $sw.Write([Fold652]::DecompressChunk($b))
    }
    $br.Close(); $sw.Close()
    (Get-Item $base).LastWriteTime = $File.LastWriteTime
    Write-Host "Decompressed -> $base"
}

# ---- main ----
Get-ChildItem $Folder -File | Where-Object Length -GE 2GB | ForEach-Object {
    if($Action -eq 'compress'){ Compress-652 $_ }
    else{ Decompress-652 $_ }
}