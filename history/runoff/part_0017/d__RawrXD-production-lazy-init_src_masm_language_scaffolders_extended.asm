; ==============================================================================
; RawrXD Agentic IDE - Extended Language Scaffolders
; ==============================================================================
; Additional scaffolders for Java, C#, Swift, Kotlin, Ruby, PHP, Perl, Lua,
; Elixir, Haskell, and more advanced languages.
;
; Author: RawrXD Team
; Date: 2026-01-16
; License: Educational Use
; ==============================================================================

; ==============================================================================
; RawrXD Agentic IDE - Extended Language Scaffolders
; ==============================================================================
; Additional scaffolders for Java, C#, Swift, Kotlin, Ruby, PHP, Perl, Lua,
; Elixir, Haskell, and more advanced languages.
;
; Author: RawrXD Team
; Date: 2026-01-16
; License: Educational Use
; ==============================================================================

; External Win32 API functions
EXTERN CreateFileA:PROC
EXTERN WriteFile:PROC
EXTERN CloseHandle:PROC
EXTERN CreateDirectoryA:PROC
EXTERN lstrcpyA:PROC
EXTERN lstrcatA:PROC
EXTERN lstrlenA:PROC

; Win32 constants
GENERIC_WRITE       EQU 40000000h
CREATE_ALWAYS       EQU 2
FILE_ATTRIBUTE_NORMAL EQU 80h
INVALID_HANDLE_VALUE EQU -1

; Constants
MAX_PATH    EQU 260

; Forward declarations from language_scaffolders.asm
EXTERN WriteFileStr:PROC
EXTERN MkDir:PROC
EXTERN szBackslash:BYTE
EXTERN szSrc:BYTE

.data

; ============================================================================
; Java Templates
; ============================================================================
szJavaMain      DB 'public class Main {', 13, 10
                DB '    public static void main(String[] args) {', 13, 10
                DB '        System.out.println("Hello from RawrXD Java!");', 13, 10
                DB '    }', 13, 10
                DB '}', 13, 10, 0

szJavaBuildGradle DB 'plugins {', 13, 10
                  DB '    id("java")', 13, 10
                  DB '    id("application")', 13, 10
                  DB '}', 13, 10, 13, 10
                  DB 'group = "com.rawrxd"', 13, 10
                  DB 'version = "1.0"', 13, 10, 13, 10
                  DB 'repositories {', 13, 10
                  DB '    mavenCentral()', 13, 10
                  DB '}', 13, 10, 13, 10
                  DB 'application {', 13, 10
                  DB '    mainClass.set("Main")', 13, 10
                  DB '}', 13, 10, 0

; ============================================================================
; C# Templates
; ============================================================================
szCSharpMain    DB 'using System;', 13, 10, 13, 10
                DB 'namespace RawrXD', 13, 10
                DB '{', 13, 10
                DB '    class Program', 13, 10
                DB '    {', 13, 10
                DB '        static void Main(string[] args)', 13, 10
                DB '        {', 13, 10
                DB '            Console.WriteLine("Hello from RawrXD C#!");', 13, 10
                DB '        }', 13, 10
                DB '    }', 13, 10
                DB '}', 13, 10, 0

szCSharpProj    DB '<Project Sdk="Microsoft.NET.Sdk">', 13, 10
                DB '  <PropertyGroup>', 13, 10
                DB '    <OutputType>Exe</OutputType>', 13, 10
                DB '    <TargetFramework>net8.0</TargetFramework>', 13, 10
                DB '    <LangVersion>latest</LangVersion>', 13, 10
                DB '    <Nullable>enable</Nullable>', 13, 10
                DB '  </PropertyGroup>', 13, 10
                DB '</Project>', 13, 10, 0

; ============================================================================
; Swift Templates
; ============================================================================
szSwiftMain     DB 'import Foundation', 13, 10, 13, 10
                DB 'print("Hello from RawrXD Swift!")', 13, 10, 0

szSwiftPackage  DB '// swift-tools-version:5.9', 13, 10
                DB 'import PackageDescription', 13, 10, 13, 10
                DB 'let package = Package(', 13, 10
                DB '    name: "RawrXD",', 13, 10
                DB '    platforms: [.macOS(.v13)],', 13, 10
                DB '    products: [', 13, 10
                DB '        .executable(name: "rawrxd", targets: ["RawrXD"])', 13, 10
                DB '    ],', 13, 10
                DB '    targets: [', 13, 10
                DB '        .executableTarget(name: "RawrXD")', 13, 10
                DB '    ]', 13, 10
                DB ')', 13, 10, 0

; ============================================================================
; Kotlin Templates
; ============================================================================
szKotlinMain    DB 'fun main() {', 13, 10
                DB '    println("Hello from RawrXD Kotlin!")', 13, 10
                DB '}', 13, 10, 0

szKotlinBuild   DB 'plugins {', 13, 10
                DB '    kotlin("jvm") version "1.9.22"', 13, 10
                DB '    application', 13, 10
                DB '}', 13, 10, 13, 10
                DB 'group = "com.rawrxd"', 13, 10
                DB 'version = "1.0"', 13, 10, 13, 10
                DB 'repositories {', 13, 10
                DB '    mavenCentral()', 13, 10
                DB '}', 13, 10, 13, 10
                DB 'dependencies {', 13, 10
                DB '    implementation(kotlin("stdlib"))', 13, 10
                DB '}', 13, 10, 13, 10
                DB 'application {', 13, 10
                DB '    mainClass.set("MainKt")', 13, 10
                DB '}', 13, 10, 0

; ============================================================================
; Ruby Templates
; ============================================================================
szRubyMain      DB '#!/usr/bin/env ruby', 13, 10, 13, 10
                DB 'def main', 13, 10
                DB '  puts "Hello from RawrXD Ruby!"', 13, 10
                DB 'end', 13, 10, 13, 10
                DB 'main if __FILE__ == $0', 13, 10, 0

szRubyGemfile   DB 'source "https://rubygems.org"', 13, 10, 13, 10
                DB 'ruby ">=3.0.0"', 13, 10, 13, 10
                DB 'gem "rake"', 13, 10
                DB 'gem "rspec"', 13, 10, 0

; ============================================================================
; PHP Templates
; ============================================================================
szPHPMain       DB '<?php', 13, 10, 13, 10
                DB 'function main() {', 13, 10
                DB '    echo "Hello from RawrXD PHP!\n";', 13, 10
                DB '}', 13, 10, 13, 10
                DB 'main();', 13, 10
                DB '?>', 13, 10, 0

szPHPComposer   DB '{', 13, 10
                DB '    "name": "rawrxd/app",', 13, 10
                DB '    "description": "RawrXD PHP Application",', 13, 10
                DB '    "type": "project",', 13, 10
                DB '    "require": {', 13, 10
                DB '        "php": ">=8.2"', 13, 10
                DB '    },', 13, 10
                DB '    "autoload": {', 13, 10
                DB '        "psr-4": {', 13, 10
                DB '            "RawrXD\\\\": "src/"', 13, 10
                DB '        }', 13, 10
                DB '    }', 13, 10
                DB '}', 13, 10, 0

; ============================================================================
; Perl Templates
; ============================================================================
szPerlMain      DB '#!/usr/bin/env perl', 13, 10
                DB 'use strict;', 13, 10
                DB 'use warnings;', 13, 10, 13, 10
                DB 'sub main {', 13, 10
                DB '    print "Hello from RawrXD Perl!\n";', 13, 10
                DB '}', 13, 10, 13, 10
                DB 'main();', 13, 10, 0

; ============================================================================
; Lua Templates
; ============================================================================
szLuaMain       DB '-- RawrXD Lua Application', 13, 10, 13, 10
                DB 'function main()', 13, 10
                DB '    print("Hello from RawrXD Lua!")', 13, 10
                DB 'end', 13, 10, 13, 10
                DB 'main()', 13, 10, 0

; ============================================================================
; Elixir Templates
; ============================================================================
szElixirMain    DB 'defmodule RawrXD do', 13, 10
                DB '  def main do', 13, 10
                DB '    IO.puts("Hello from RawrXD Elixir!")', 13, 10
                DB '  end', 13, 10
                DB 'end', 13, 10, 13, 10
                DB 'RawrXD.main()', 13, 10, 0

szElixirMix     DB 'defmodule RawrXD.MixProject do', 13, 10
                DB '  use Mix.Project', 13, 10, 13, 10
                DB '  def project do', 13, 10
                DB '    [', 13, 10
                DB '      app: :rawrxd,', 13, 10
                DB '      version: "0.1.0",', 13, 10
                DB '      elixir: "~> 1.15",', 13, 10
                DB '      start_permanent: Mix.env() == :prod,', 13, 10
                DB '      deps: deps()', 13, 10
                DB '    ]', 13, 10
                DB '  end', 13, 10, 13, 10
                DB '  def application do', 13, 10
                DB '    [extra_applications: [:logger]]', 13, 10
                DB '  end', 13, 10, 13, 10
                DB '  defp deps do', 13, 10
                DB '    []', 13, 10
                DB '  end', 13, 10
                DB 'end', 13, 10, 0

; ============================================================================
; Haskell Templates
; ============================================================================
szHaskellMain   DB 'module Main where', 13, 10, 13, 10
                DB 'main :: IO ()', 13, 10
                DB 'main = putStrLn "Hello from RawrXD Haskell!"', 13, 10, 0

szHaskellCabal  DB 'cabal-version:       3.0', 13, 10
                DB 'name:                rawrxd', 13, 10
                DB 'version:             0.1.0.0', 13, 10
                DB 'build-type:          Simple', 13, 10, 13, 10
                DB 'executable rawrxd', 13, 10
                DB '  main-is:           Main.hs', 13, 10
                DB '  build-depends:     base >=4.16', 13, 10
                DB '  default-language:  Haskell2010', 13, 10, 0

; ============================================================================
; OCaml Templates
; ============================================================================
szOCamlMain     DB 'let () =', 13, 10
                DB '  print_endline "Hello from RawrXD OCaml!"', 13, 10, 0

szOCamlDune     DB '(executable', 13, 10
                DB ' (name main)', 13, 10
                DB ' (libraries base))', 13, 10, 0

; ============================================================================
; Scala Templates
; ============================================================================
szScalaMain     DB 'object Main {', 13, 10
                DB '  def main(args: Array[String]): Unit = {', 13, 10
                DB '    println("Hello from RawrXD Scala!")', 13, 10
                DB '  }', 13, 10
                DB '}', 13, 10, 0

szScalaBuild    DB 'name := "RawrXD"', 13, 10
                DB 'version := "0.1"', 13, 10
                DB 'scalaVersion := "3.3.1"', 13, 10, 0

; ============================================================================
; File Name Constants
; ============================================================================
szMainJava      DB 'Main.java', 0
szBuildGradle   DB 'build.gradle', 0
szMainCS        DB 'Program.cs', 0
szCSProj        DB 'RawrXD.csproj', 0
szMainSwift     DB 'main.swift', 0
szPackageSwift  DB 'Package.swift', 0
szSources       DB 'Sources', 0
szRawrXD        DB 'RawrXD', 0
szMainKt        DB 'Main.kt', 0
szBuildGradleKts DB 'build.gradle.kts', 0
szMain          DB 'main', 0
szKotlin        DB 'kotlin', 0
szMainRb        DB 'main.rb', 0
szGemfile       DB 'Gemfile', 0
szMainPHP       DB 'index.php', 0
szComposerJson  DB 'composer.json', 0
szMainPl        DB 'main.pl', 0
szMainLua       DB 'main.lua', 0
szMainEx        DB 'main.ex', 0
szMixExs        DB 'mix.exs', 0
szLib           DB 'lib', 0
szMainHs        DB 'Main.hs', 0
szRawrxdCabal   DB 'rawrxd.cabal', 0
szMainMl        DB 'main.ml', 0
szDuneFile      DB 'dune', 0
szMainScala     DB 'Main.scala', 0
szBuildSbt      DB 'build.sbt', 0

.code

; ============================================================================
; Java Scaffolder
; ============================================================================
ScaffoldJava PROC targetPath:QWORD
    LOCAL filePath[MAX_PATH]:BYTE
    LOCAL srcPath[MAX_PATH]:BYTE
    LOCAL mainPath[MAX_PATH]:BYTE
    LOCAL javaPath[MAX_PATH]:BYTE
    
    ; Create target directory
    invoke  MkDir, targetPath
    
    ; Create src/main/java directory structure
    invoke  lstrcpyA, ADDR srcPath, targetPath
    invoke  lstrcatA, ADDR srcPath, OFFSET szBackslash
    invoke  lstrcatA, ADDR srcPath, OFFSET szSrc
    invoke  MkDir, ADDR srcPath
    
    invoke  lstrcpyA, ADDR mainPath, ADDR srcPath
    invoke  lstrcatA, ADDR mainPath, OFFSET szBackslash
    invoke  lstrcatA, ADDR mainPath, OFFSET szMain
    invoke  MkDir, ADDR mainPath
    
    invoke  lstrcpyA, ADDR javaPath, ADDR mainPath
    invoke  lstrcatA, ADDR javaPath, OFFSET szBackslash
    DB 'java', 0  ; Inline constant
    invoke  MkDir, ADDR javaPath
    
    ; Write Main.java
    invoke  lstrcpyA, ADDR filePath, ADDR javaPath
    invoke  lstrcatA, ADDR filePath, OFFSET szBackslash
    invoke  lstrcatA, ADDR filePath, OFFSET szMainJava
    invoke  WriteFileStr, ADDR filePath, OFFSET szJavaMain
    
    ; Write build.gradle
    invoke  lstrcpyA, ADDR filePath, targetPath
    invoke  lstrcatA, ADDR filePath, OFFSET szBackslash
    invoke  lstrcatA, ADDR filePath, OFFSET szBuildGradle
    invoke  WriteFileStr, ADDR filePath, OFFSET szJavaBuildGradle
    
    ret
ScaffoldJava ENDP

; ============================================================================
; C# Scaffolder
; ============================================================================
ScaffoldCSharp PROC targetPath:QWORD
    LOCAL filePath[MAX_PATH]:BYTE
    
    ; Create target directory
    invoke  MkDir, targetPath
    
    ; Write Program.cs
    invoke  lstrcpyA, ADDR filePath, targetPath
    invoke  lstrcatA, ADDR filePath, OFFSET szBackslash
    invoke  lstrcatA, ADDR filePath, OFFSET szMainCS
    invoke  WriteFileStr, ADDR filePath, OFFSET szCSharpMain
    
    ; Write .csproj
    invoke  lstrcpyA, ADDR filePath, targetPath
    invoke  lstrcatA, ADDR filePath, OFFSET szBackslash
    invoke  lstrcatA, ADDR filePath, OFFSET szCSProj
    invoke  WriteFileStr, ADDR filePath, OFFSET szCSharpProj
    
    ret
ScaffoldCSharp ENDP

; ============================================================================
; Swift Scaffolder
; ============================================================================
ScaffoldSwift PROC targetPath:QWORD
    LOCAL filePath[MAX_PATH]:BYTE
    LOCAL sourcesPath[MAX_PATH]:BYTE
    LOCAL rawrxdPath[MAX_PATH]:BYTE
    
    ; Create target directory
    invoke  MkDir, targetPath
    
    ; Create Sources directory
    invoke  lstrcpyA, ADDR sourcesPath, targetPath
    invoke  lstrcatA, ADDR sourcesPath, OFFSET szBackslash
    invoke  lstrcatA, ADDR sourcesPath, OFFSET szSources
    invoke  MkDir, ADDR sourcesPath
    
    ; Create Sources/RawrXD directory
    invoke  lstrcpyA, ADDR rawrxdPath, ADDR sourcesPath
    invoke  lstrcatA, ADDR rawrxdPath, OFFSET szBackslash
    invoke  lstrcatA, ADDR rawrxdPath, OFFSET szRawrXD
    invoke  MkDir, ADDR rawrxdPath
    
    ; Write Sources/RawrXD/main.swift
    invoke  lstrcpyA, ADDR filePath, ADDR rawrxdPath
    invoke  lstrcatA, ADDR filePath, OFFSET szBackslash
    invoke  lstrcatA, ADDR filePath, OFFSET szMainSwift
    invoke  WriteFileStr, ADDR filePath, OFFSET szSwiftMain
    
    ; Write Package.swift
    invoke  lstrcpyA, ADDR filePath, targetPath
    invoke  lstrcatA, ADDR filePath, OFFSET szBackslash
    invoke  lstrcatA, ADDR filePath, OFFSET szPackageSwift
    invoke  WriteFileStr, ADDR filePath, OFFSET szSwiftPackage
    
    ret
ScaffoldSwift ENDP

; ============================================================================
; Kotlin Scaffolder
; ============================================================================
ScaffoldKotlin PROC targetPath:QWORD
    LOCAL filePath[MAX_PATH]:BYTE
    LOCAL srcPath[MAX_PATH]:BYTE
    LOCAL mainPath[MAX_PATH]:BYTE
    LOCAL kotlinPath[MAX_PATH]:BYTE
    
    ; Create target directory
    invoke  MkDir, targetPath
    
    ; Create src/main/kotlin directory structure
    invoke  lstrcpyA, ADDR srcPath, targetPath
    invoke  lstrcatA, ADDR srcPath, OFFSET szBackslash
    invoke  lstrcatA, ADDR srcPath, OFFSET szSrc
    invoke  MkDir, ADDR srcPath
    
    invoke  lstrcpyA, ADDR mainPath, ADDR srcPath
    invoke  lstrcatA, ADDR mainPath, OFFSET szBackslash
    invoke  lstrcatA, ADDR mainPath, OFFSET szMain
    invoke  MkDir, ADDR mainPath
    
    invoke  lstrcpyA, ADDR kotlinPath, ADDR mainPath
    invoke  lstrcatA, ADDR kotlinPath, OFFSET szBackslash
    invoke  lstrcatA, ADDR kotlinPath, OFFSET szKotlin
    invoke  MkDir, ADDR kotlinPath
    
    ; Write Main.kt
    invoke  lstrcpyA, ADDR filePath, ADDR kotlinPath
    invoke  lstrcatA, ADDR filePath, OFFSET szBackslash
    invoke  lstrcatA, ADDR filePath, OFFSET szMainKt
    invoke  WriteFileStr, ADDR filePath, OFFSET szKotlinMain
    
    ; Write build.gradle.kts
    invoke  lstrcpyA, ADDR filePath, targetPath
    invoke  lstrcatA, ADDR filePath, OFFSET szBackslash
    invoke  lstrcatA, ADDR filePath, OFFSET szBuildGradleKts
    invoke  WriteFileStr, ADDR filePath, OFFSET szKotlinBuild
    
    ret
ScaffoldKotlin ENDP

; ============================================================================
; Ruby Scaffolder
; ============================================================================
ScaffoldRuby PROC targetPath:QWORD
    LOCAL filePath[MAX_PATH]:BYTE
    
    ; Create target directory
    invoke  MkDir, targetPath
    
    ; Write main.rb
    invoke  lstrcpyA, ADDR filePath, targetPath
    invoke  lstrcatA, ADDR filePath, OFFSET szBackslash
    invoke  lstrcatA, ADDR filePath, OFFSET szMainRb
    invoke  WriteFileStr, ADDR filePath, OFFSET szRubyMain
    
    ; Write Gemfile
    invoke  lstrcpyA, ADDR filePath, targetPath
    invoke  lstrcatA, ADDR filePath, OFFSET szBackslash
    invoke  lstrcatA, ADDR filePath, OFFSET szGemfile
    invoke  WriteFileStr, ADDR filePath, OFFSET szRubyGemfile
    
    ret
ScaffoldRuby ENDP

; ============================================================================
; PHP Scaffolder
; ============================================================================
ScaffoldPHP PROC targetPath:QWORD
    LOCAL filePath[MAX_PATH]:BYTE
    LOCAL srcPath[MAX_PATH]:BYTE
    
    ; Create target directory
    invoke  MkDir, targetPath
    
    ; Create src directory
    invoke  lstrcpyA, ADDR srcPath, targetPath
    invoke  lstrcatA, ADDR srcPath, OFFSET szBackslash
    invoke  lstrcatA, ADDR srcPath, OFFSET szSrc
    invoke  MkDir, ADDR srcPath
    
    ; Write index.php
    invoke  lstrcpyA, ADDR filePath, targetPath
    invoke  lstrcatA, ADDR filePath, OFFSET szBackslash
    invoke  lstrcatA, ADDR filePath, OFFSET szMainPHP
    invoke  WriteFileStr, ADDR filePath, OFFSET szPHPMain
    
    ; Write composer.json
    invoke  lstrcpyA, ADDR filePath, targetPath
    invoke  lstrcatA, ADDR filePath, OFFSET szBackslash
    invoke  lstrcatA, ADDR filePath, OFFSET szComposerJson
    invoke  WriteFileStr, ADDR filePath, OFFSET szPHPComposer
    
    ret
ScaffoldPHP ENDP

; ============================================================================
; Perl Scaffolder
; ============================================================================
ScaffoldPerl PROC targetPath:QWORD
    LOCAL filePath[MAX_PATH]:BYTE
    
    ; Create target directory
    invoke  MkDir, targetPath
    
    ; Write main.pl
    invoke  lstrcpyA, ADDR filePath, targetPath
    invoke  lstrcatA, ADDR filePath, OFFSET szBackslash
    invoke  lstrcatA, ADDR filePath, OFFSET szMainPl
    invoke  WriteFileStr, ADDR filePath, OFFSET szPerlMain
    
    ret
ScaffoldPerl ENDP

; ============================================================================
; Lua Scaffolder
; ============================================================================
ScaffoldLua PROC targetPath:QWORD
    LOCAL filePath[MAX_PATH]:BYTE
    
    ; Create target directory
    invoke  MkDir, targetPath
    
    ; Write main.lua
    invoke  lstrcpyA, ADDR filePath, targetPath
    invoke  lstrcatA, ADDR filePath, OFFSET szBackslash
    invoke  lstrcatA, ADDR filePath, OFFSET szMainLua
    invoke  WriteFileStr, ADDR filePath, OFFSET szLuaMain
    
    ret
ScaffoldLua ENDP

; ============================================================================
; Elixir Scaffolder
; ============================================================================
ScaffoldElixir PROC targetPath:QWORD
    LOCAL filePath[MAX_PATH]:BYTE
    LOCAL libPath[MAX_PATH]:BYTE
    
    ; Create target directory
    invoke  MkDir, targetPath
    
    ; Create lib directory
    invoke  lstrcpyA, ADDR libPath, targetPath
    invoke  lstrcatA, ADDR libPath, OFFSET szBackslash
    invoke  lstrcatA, ADDR libPath, OFFSET szLib
    invoke  MkDir, ADDR libPath
    
    ; Write lib/main.ex
    invoke  lstrcpyA, ADDR filePath, ADDR libPath
    invoke  lstrcatA, ADDR filePath, OFFSET szBackslash
    invoke  lstrcatA, ADDR filePath, OFFSET szMainEx
    invoke  WriteFileStr, ADDR filePath, OFFSET szElixirMain
    
    ; Write mix.exs
    invoke  lstrcpyA, ADDR filePath, targetPath
    invoke  lstrcatA, ADDR filePath, OFFSET szBackslash
    invoke  lstrcatA, ADDR filePath, OFFSET szMixExs
    invoke  WriteFileStr, ADDR filePath, OFFSET szElixirMix
    
    ret
ScaffoldElixir ENDP

; ============================================================================
; Haskell Scaffolder
; ============================================================================
ScaffoldHaskell PROC targetPath:QWORD
    LOCAL filePath[MAX_PATH]:BYTE
    
    ; Create target directory
    invoke  MkDir, targetPath
    
    ; Write Main.hs
    invoke  lstrcpyA, ADDR filePath, targetPath
    invoke  lstrcatA, ADDR filePath, OFFSET szBackslash
    invoke  lstrcatA, ADDR filePath, OFFSET szMainHs
    invoke  WriteFileStr, ADDR filePath, OFFSET szHaskellMain
    
    ; Write rawrxd.cabal
    invoke  lstrcpyA, ADDR filePath, targetPath
    invoke  lstrcatA, ADDR filePath, OFFSET szBackslash
    invoke  lstrcatA, ADDR filePath, OFFSET szRawrxdCabal
    invoke  WriteFileStr, ADDR filePath, OFFSET szHaskellCabal
    
    ret
ScaffoldHaskell ENDP

; ============================================================================
; OCaml Scaffolder
; ============================================================================
ScaffoldOCaml PROC targetPath:QWORD
    LOCAL filePath[MAX_PATH]:BYTE
    
    ; Create target directory
    invoke  MkDir, targetPath
    
    ; Write main.ml
    invoke  lstrcpyA, ADDR filePath, targetPath
    invoke  lstrcatA, ADDR filePath, OFFSET szBackslash
    invoke  lstrcatA, ADDR filePath, OFFSET szMainMl
    invoke  WriteFileStr, ADDR filePath, OFFSET szOCamlMain
    
    ; Write dune
    invoke  lstrcpyA, ADDR filePath, targetPath
    invoke  lstrcatA, ADDR filePath, OFFSET szBackslash
    invoke  lstrcatA, ADDR filePath, OFFSET szDuneFile
    invoke  WriteFileStr, ADDR filePath, OFFSET szOCamlDune
    
    ret
ScaffoldOCaml ENDP

; ============================================================================
; Scala Scaffolder
; ============================================================================
ScaffoldScala PROC targetPath:QWORD
    LOCAL filePath[MAX_PATH]:BYTE
    LOCAL srcPath[MAX_PATH]:BYTE
    LOCAL mainPath[MAX_PATH]:BYTE
    LOCAL scalaPath[MAX_PATH]:BYTE
    
    ; Create target directory
    invoke  MkDir, targetPath
    
    ; Create src/main/scala directory structure
    invoke  lstrcpyA, ADDR srcPath, targetPath
    invoke  lstrcatA, ADDR srcPath, OFFSET szBackslash
    invoke  lstrcatA, ADDR srcPath, OFFSET szSrc
    invoke  MkDir, ADDR srcPath
    
    invoke  lstrcpyA, ADDR mainPath, ADDR srcPath
    invoke  lstrcatA, ADDR mainPath, OFFSET szBackslash
    invoke  lstrcatA, ADDR mainPath, OFFSET szMain
    invoke  MkDir, ADDR mainPath
    
    invoke  lstrcpyA, ADDR scalaPath, ADDR mainPath
    invoke  lstrcatA, ADDR scalaPath, OFFSET szBackslash
    DB 'scala', 0  ; Inline constant
    invoke  MkDir, ADDR scalaPath
    
    ; Write Main.scala
    invoke  lstrcpyA, ADDR filePath, ADDR scalaPath
    invoke  lstrcatA, ADDR filePath, OFFSET szBackslash
    invoke  lstrcatA, ADDR filePath, OFFSET szMainScala
    invoke  WriteFileStr, ADDR filePath, OFFSET szScalaMain
    
    ; Write build.sbt
    invoke  lstrcpyA, ADDR filePath, targetPath
    invoke  lstrcatA, ADDR filePath, OFFSET szBackslash
    invoke  lstrcatA, ADDR filePath, OFFSET szBuildSbt
    invoke  WriteFileStr, ADDR filePath, OFFSET szScalaBuild
    
    ret
ScaffoldScala ENDP

; ============================================================================
; Exports
; ============================================================================
PUBLIC  ScaffoldJava
PUBLIC  ScaffoldCSharp
PUBLIC  ScaffoldSwift
PUBLIC  ScaffoldKotlin
PUBLIC  ScaffoldRuby
PUBLIC  ScaffoldPHP
PUBLIC  ScaffoldPerl
PUBLIC  ScaffoldLua
PUBLIC  ScaffoldElixir
PUBLIC  ScaffoldHaskell
PUBLIC  ScaffoldOCaml
PUBLIC  ScaffoldScala

END
