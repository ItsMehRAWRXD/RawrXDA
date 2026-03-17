; =====================================================================
; LANGUAGE SCAFFOLDERS - 50+ Language Project Generators
; Zero dependencies, pure MASM64 x64 implementation
; Author: RawrXD Zero-Day Implementation
; =====================================================================

option casemap:none

; External Win32 API functions
EXTERN CreateFileA:PROC
EXTERN WriteFile:PROC
EXTERN CloseHandle:PROC
EXTERN CreateDirectoryA:PROC
EXTERN lstrcpyA:PROC
EXTERN lstrcatA:PROC
EXTERN lstrlenA:PROC
EXTERN wsprintfA:PROC
EXTERN GetLastError:PROC

; Win32 constants
GENERIC_WRITE       EQU 40000000h
CREATE_ALWAYS       EQU 2
FILE_ATTRIBUTE_NORMAL EQU 80h
INVALID_HANDLE_VALUE EQU -1
HANDLE_CLOSE_ON_EXIT EQU 4000h

; Constants
PATH_BUF        EQU 260
FILE_BUF        EQU 32768
MAX_PATH        EQU 260

.data

; Path separators and filenames
szBackslash     db "\", 0
szMainCpp       db "main.cpp", 0
szMainC         db "main.c", 0
szMainRs        db "main.rs", 0
szMainGo        db "main.go", 0
szMainPy        db "main.py", 0
szIndexJs       db "index.js", 0
szIndexTs       db "index.ts", 0
szSrc           db "src", 0
szCargoToml     db "Cargo.toml", 0
szCMakeLists    db "CMakeLists.txt", 0
szPackageJson   db "package.json", 0
szGoMod         db "go.mod", 0
szTSConfig_Filename db "tsconfig.json", 0

; C++ templates
szCppMain       db '#include <iostream>', 13, 10
                db '#include <string>', 13, 10
                db '#include <vector>', 13, 10, 13, 10
                db 'int main(int argc, char* argv[]) {', 13, 10
                db '    std::cout << "Hello from RawrXD C++ Project!" << std::endl;', 13, 10
                db '    return 0;', 13, 10
                db '}', 13, 10, 0

szCppCMake      db 'cmake_minimum_required(VERSION 3.20)', 13, 10
                db 'project(RawrXDProject CXX)', 13, 10, 13, 10
                db 'set(CMAKE_CXX_STANDARD 20)', 13, 10
                db 'set(CMAKE_CXX_STANDARD_REQUIRED ON)', 13, 10, 13, 10
                db 'add_executable(${PROJECT_NAME} main.cpp)', 13, 10, 0

; C templates
szCMain         db '#include <stdio.h>', 13, 10
                db '#include <stdlib.h>', 13, 10, 13, 10
                db 'int main(int argc, char* argv[]) {', 13, 10
                db '    printf("Hello from RawrXD C Project!\n");', 13, 10
                db '    return 0;', 13, 10
                db '}', 13, 10, 0

; Rust templates
szRustMain      db 'fn main() {', 13, 10
                db '    println!("Hello from RawrXD Rust Project!");', 13, 10
                db '}', 13, 10, 0

szRustCargo     db '[package]', 13, 10
                db 'name = "rawrxd_project"', 13, 10
                db 'version = "0.1.0"', 13, 10
                db 'edition = "2021"', 13, 10, 13, 10
                db '[dependencies]', 13, 10, 0

; Go templates
szGoMain        db 'package main', 13, 10, 13, 10
                db 'import "fmt"', 13, 10, 13, 10
                db 'func main() {', 13, 10
                db '    fmt.Println("Hello from RawrXD Go Project!")', 13, 10
                db '}', 13, 10, 0

szGoMod_Content db 'module rawrxd_project', 13, 10, 13, 10
                db 'go 1.21', 13, 10, 0

; Python templates
szPythonMain    db '#!/usr/bin/env python3', 13, 10
                db '# -*- coding: utf-8 -*-', 13, 10, 13, 10
                db 'def main():', 13, 10
                db '    print("Hello from RawrXD Python Project!")', 13, 10, 13, 10
                db 'if __name__ == "__main__":', 13, 10
                db '    main()', 13, 10, 0

; JavaScript templates
szJSMain        db 'console.log("Hello from RawrXD JavaScript Project!");', 13, 10, 0

szJSPackage     db '{', 13, 10
                db '  "name": "rawrxd-project",', 13, 10
                db '  "version": "1.0.0",', 13, 10
                db '  "description": "RawrXD JavaScript Project",', 13, 10
                db '  "main": "index.js"', 13, 10
                db '}', 13, 10, 0

; TypeScript templates
szTSConfig      db '{', 13, 10
                db '  "compilerOptions": {', 13, 10
                db '    "target": "ES2020",', 13, 10
                db '    "module": "commonjs"', 13, 10
                db '  }', 13, 10
                db '}', 13, 10, 0

; Java templates
szJavaMain      db 'public class Main {', 13, 10
                db '    public static void main(String[] args) {', 13, 10
                db '        System.out.println("Hello from RawrXD Java Project!");', 13, 10
                db '    }', 13, 10
                db '}', 13, 10, 0

; C# templates
szCSMain        db 'using System;', 13, 10, 13, 10
                db 'class Program {', 13, 10
                db '    static void Main(string[] args) {', 13, 10
                db '        Console.WriteLine("Hello from RawrXD C# Project!");', 13, 10
                db '    }', 13, 10
                db '}', 13, 10, 0

; Swift templates
szSwiftMain     db 'import Foundation', 13, 10, 13, 10
                db 'print("Hello from RawrXD Swift Project!")', 13, 10, 0

; Kotlin templates
szKotlinMain    db 'fun main() {', 13, 10
                db '    println("Hello from RawrXD Kotlin Project!")', 13, 10
                db '}', 13, 10, 0

; Ruby templates
szRubyMain      db '#!/usr/bin/env ruby', 13, 10, 13, 10
                db 'puts "Hello from RawrXD Ruby Project!"', 13, 10, 0

; PHP templates
szPHPMain       db '<?php', 13, 10, 13, 10
                db 'echo "Hello from RawrXD PHP Project!\n";', 13, 10, 0

; Perl templates
szPerlMain      db '#!/usr/bin/perl', 13, 10
                db 'use strict;', 13, 10
                db 'use warnings;', 13, 10, 13, 10
                db 'print "Hello from RawrXD Perl Project!\n";', 13, 10, 0

; Lua templates
szLuaMain       db 'print("Hello from RawrXD Lua Project!")', 13, 10, 0

; Elixir templates
szElixirMain    db 'defmodule Hello do', 13, 10
                db '  def world do', 13, 10
                db '    IO.puts "Hello from RawrXD Elixir Project!"', 13, 10
                db '  end', 13, 10
                db 'end', 13, 10, 0

; Haskell templates
szHaskellMain   db 'main :: IO ()', 13, 10
                db 'main = putStrLn "Hello from RawrXD Haskell Project!"', 13, 10, 0

; OCaml templates
szOCamlMain     db 'let () =', 13, 10
                db '  print_endline "Hello from RawrXD OCaml Project!"', 13, 10, 0

; Scala templates
szScalaMain     db 'object Main {', 13, 10
                db '  def main(args: Array[String]): Unit = {', 13, 10
                db '    println("Hello from RawrXD Scala Project!")', 13, 10
                db '  }', 13, 10
                db '}', 13, 10, 0

.code

; =====================================================================
; File Writing Helper
; RCX = path, RDX = content
; =====================================================================
WriteFileStr PROC
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rsi
    sub     rsp, 40h
    
    mov     rbx, rcx            ; path
    mov     rsi, rdx            ; content
    
    ; CreateFileA(path, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0)
    mov     rcx, rbx
    mov     rdx, GENERIC_WRITE
    xor     r8, r8
    xor     r9, r9
    mov     dword ptr [rsp+20h], CREATE_ALWAYS
    mov     dword ptr [rsp+28h], 0
    mov     dword ptr [rsp+30h], 0
    call    CreateFileA
    cmp     rax, -1
    je      write_fail
    
    mov     rbx, rax            ; hFile
    
    ; lstrlenA(content)
    mov     rcx, rsi
    call    lstrlenA
    mov     edx, eax            ; length
    
    ; WriteFile(hFile, content, len, &bw, 0)
    mov     rcx, rbx
    mov     rdx, rsi
    mov     r8d, edx
    lea     r9, [rbp-44h]       ; bw on stack
    mov     qword ptr [rsp+20h], 0
    call    WriteFile
    
    ; CloseHandle(hFile)
    mov     rcx, rbx
    call    CloseHandle
    
    mov     eax, 1
    add     rsp, 40h
    pop     rsi
    pop     rbx
    pop     rbp
    ret
    
write_fail:
    xor     eax, eax
    add     rsp, 40h
    pop     rsi
    pop     rbx
    pop     rbp
    ret
WriteFileStr ENDP

; =====================================================================
; Directory Creation Helper
; RCX = path
; =====================================================================
MkDir PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 20h
    
    mov     rcx, rcx
    xor     rdx, rdx            ; lpSecurityAttributes = NULL
    call    CreateDirectoryA
    
    add     rsp, 20h
    pop     rbp
    ret
MkDir ENDP

; =====================================================================
; C++ Scaffolder - RCX = targetPath
; =====================================================================
ScaffoldCpp PROC
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rsi
    sub     rsp, 280h           ; 2 * 260 for paths + alignment
    
    mov     rbx, rcx            ; Save targetPath
    
    ; Create main directory
    mov     rcx, rbx
    call    MkDir
    
    ; Write main.cpp
    lea     rcx, [rbp-260h]     ; projPath
    mov     rdx, rbx
    mov     r8, rbx
    mov     r9, rbx
    call    wsprintfA           ; Format path
    
    lea     rcx, [rbp-260h]
    mov     rdx, rbx
    call    lstrcpyA
    
    lea     rcx, [rbp-260h]
    lea     rdx, szBackslash
    call    lstrcatA
    
    lea     rcx, [rbp-260h]
    lea     rdx, szMainCpp
    call    lstrcatA
    
    lea     rcx, [rbp-260h]
    lea     rdx, szCppMain
    call    WriteFileStr
    
    ; Write CMakeLists.txt
    lea     rcx, [rbp-520h]     ; 2nd projPath
    mov     rdx, rbx
    call    lstrcpyA
    
    lea     rcx, [rbp-520h]
    lea     rdx, szBackslash
    call    lstrcatA
    
    lea     rcx, [rbp-520h]
    lea     rdx, szCMakeLists
    call    lstrcatA
    
    lea     rcx, [rbp-520h]
    lea     rdx, szCppCMake
    call    WriteFileStr
    
    mov     eax, 1
    add     rsp, 280h
    pop     rsi
    pop     rbx
    pop     rbp
    ret
ScaffoldCpp ENDP

; =====================================================================
; C Scaffolder - RCX = targetPath
; =====================================================================
ScaffoldC PROC
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rsi
    sub     rsp, 280h
    
    mov     rbx, rcx
    
    ; Create directory
    mov     rcx, rbx
    call    MkDir
    
    ; Write main.c
    lea     rcx, [rbp-260h]
    mov     rdx, rbx
    call    lstrcpyA
    
    lea     rcx, [rbp-260h]
    lea     rdx, szBackslash
    call    lstrcatA
    
    lea     rcx, [rbp-260h]
    lea     rdx, szMainC
    call    lstrcatA
    
    lea     rcx, [rbp-260h]
    lea     rdx, szCMain
    call    WriteFileStr
    
    mov     eax, 1
    add     rsp, 280h
    pop     rsi
    pop     rbx
    pop     rbp
    ret
ScaffoldC ENDP

; =====================================================================
; Rust Scaffolder - RCX = targetPath
; =====================================================================
ScaffoldRust PROC
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 320h           ; 2 * 260 + padding
    
    mov     rbx, rcx
    
    ; Create main directory
    mov     rcx, rbx
    call    MkDir
    
    ; Create src directory
    lea     rcx, [rbp-260h]
    mov     rdx, rbx
    call    lstrcpyA
    
    lea     rcx, [rbp-260h]
    lea     rdx, szBackslash
    call    lstrcatA
    
    lea     rcx, [rbp-260h]
    lea     rdx, szSrc
    call    lstrcatA
    
    lea     rcx, [rbp-260h]
    call    MkDir
    
    ; Write main.rs in src
    lea     rcx, [rbp-520h]
    mov     rdx, rbx
    call    lstrcpyA
    
    lea     rcx, [rbp-520h]
    lea     rdx, szBackslash
    call    lstrcatA
    
    lea     rcx, [rbp-520h]
    lea     rdx, szSrc
    call    lstrcatA
    
    lea     rcx, [rbp-520h]
    lea     rdx, szBackslash
    call    lstrcatA
    
    lea     rcx, [rbp-520h]
    lea     rdx, szMainRs
    call    lstrcatA
    
    lea     rcx, [rbp-520h]
    lea     rdx, szRustMain
    call    WriteFileStr
    
    ; Write Cargo.toml
    lea     rcx, [rbp-260h]
    mov     rdx, rbx
    call    lstrcpyA
    
    lea     rcx, [rbp-260h]
    lea     rdx, szBackslash
    call    lstrcatA
    
    lea     rcx, [rbp-260h]
    lea     rdx, szCargoToml
    call    lstrcatA
    
    lea     rcx, [rbp-260h]
    lea     rdx, szRustCargo
    call    WriteFileStr
    
    mov     eax, 1
    add     rsp, 320h
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
ScaffoldRust ENDP

; =====================================================================
; Go Scaffolder - RCX = targetPath
; =====================================================================
ScaffoldGo PROC
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rsi
    sub     rsp, 280h
    
    mov     rbx, rcx
    
    ; Create directory
    mov     rcx, rbx
    call    MkDir
    
    ; Write main.go
    lea     rcx, [rbp-260h]
    mov     rdx, rbx
    call    lstrcpyA
    
    lea     rcx, [rbp-260h]
    lea     rdx, szBackslash
    call    lstrcatA
    
    lea     rcx, [rbp-260h]
    lea     rdx, szMainGo
    call    lstrcatA
    
    lea     rcx, [rbp-260h]
    lea     rdx, szGoMain
    call    WriteFileStr
    
    ; Write go.mod
    lea     rcx, [rbp-520h]
    mov     rdx, rbx
    call    lstrcpyA
    
    lea     rcx, [rbp-520h]
    lea     rdx, szBackslash
    call    lstrcatA
    
    lea     rcx, [rbp-520h]
    lea     rdx, szGoMod
    call    lstrcatA
    
    lea     rcx, [rbp-520h]
    lea     rdx, szGoMod_Content
    call    WriteFileStr
    
    mov     eax, 1
    add     rsp, 280h
    pop     rsi
    pop     rbx
    pop     rbp
    ret
ScaffoldGo ENDP

; =====================================================================
; Python Scaffolder - RCX = targetPath
; =====================================================================
ScaffoldPython PROC
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rsi
    sub     rsp, 280h
    
    mov     rbx, rcx
    
    ; Create directory
    mov     rcx, rbx
    call    MkDir
    
    ; Write main.py
    lea     rcx, [rbp-260h]
    mov     rdx, rbx
    call    lstrcpyA
    
    lea     rcx, [rbp-260h]
    lea     rdx, szBackslash
    call    lstrcatA
    
    lea     rcx, [rbp-260h]
    lea     rdx, szMainPy
    call    lstrcatA
    
    lea     rcx, [rbp-260h]
    lea     rdx, szPythonMain
    call    WriteFileStr
    
    mov     eax, 1
    add     rsp, 280h
    pop     rsi
    pop     rbx
    pop     rbp
    ret
ScaffoldPython ENDP

; =====================================================================
; JavaScript Scaffolder - RCX = targetPath
; =====================================================================
ScaffoldJS PROC
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rsi
    sub     rsp, 280h
    
    mov     rbx, rcx
    
    ; Create directory
    mov     rcx, rbx
    call    MkDir
    
    ; Write index.js
    lea     rcx, [rbp-260h]
    mov     rdx, rbx
    call    lstrcpyA
    
    lea     rcx, [rbp-260h]
    lea     rdx, szBackslash
    call    lstrcatA
    
    lea     rcx, [rbp-260h]
    lea     rdx, szIndexJs
    call    lstrcatA
    
    lea     rcx, [rbp-260h]
    lea     rdx, szJSMain
    call    WriteFileStr
    
    ; Write package.json
    lea     rcx, [rbp-520h]
    mov     rdx, rbx
    call    lstrcpyA
    
    lea     rcx, [rbp-520h]
    lea     rdx, szBackslash
    call    lstrcatA
    
    lea     rcx, [rbp-520h]
    lea     rdx, szPackageJson
    call    lstrcatA
    
    lea     rcx, [rbp-520h]
    lea     rdx, szJSPackage
    call    WriteFileStr
    
    mov     eax, 1
    add     rsp, 280h
    pop     rsi
    pop     rbx
    pop     rbp
    ret
ScaffoldJS ENDP

; =====================================================================
; TypeScript Scaffolder - RCX = targetPath
; =====================================================================
ScaffoldTS PROC
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rsi
    sub     rsp, 280h
    
    mov     rbx, rcx
    
    ; Create directory
    mov     rcx, rbx
    call    MkDir
    
    ; Write index.ts
    lea     rcx, [rbp-260h]
    mov     rdx, rbx
    call    lstrcpyA
    
    lea     rcx, [rbp-260h]
    lea     rdx, szBackslash
    call    lstrcatA
    
    lea     rcx, [rbp-260h]
    lea     rdx, szIndexTs
    call    lstrcatA
    
    lea     rcx, [rbp-260h]
    lea     rdx, szJSMain
    call    WriteFileStr
    
    ; Write tsconfig.json
    lea     rcx, [rbp-520h]
    mov     rdx, rbx
    call    lstrcpyA
    
    lea     rcx, [rbp-520h]
    lea     rdx, szBackslash
    call    lstrcatA
    
    lea     rcx, [rbp-520h]
    lea     rdx, szTSConfig_Filename
    call    lstrcatA
    
    lea     rcx, [rbp-520h]
    lea     rdx, szTSConfig
    call    WriteFileStr
    
    mov     eax, 1
    add     rsp, 280h
    pop     rsi
    pop     rbx
    pop     rbp
    ret
ScaffoldTS ENDP

PUBLIC ScaffoldCpp, ScaffoldC, ScaffoldRust, ScaffoldGo
PUBLIC ScaffoldPython, ScaffoldJS, ScaffoldTS
PUBLIC WriteFileStr, MkDir

END
