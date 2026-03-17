const crypto = require('crypto');

/**
 * YARA-style signature engine for malware detection
 * Implements pattern matching similar to YARA rules
 */
class SignatureEngine {
  constructor() {
    // Real malware signatures (simplified YARA-style rules)
    this.signatures = [
      // Ransomware signatures
      {
        name: 'Ransomware.WannaCry',
        severity: 'critical',
        category: 'ransomware',
        strings: [
          Buffer.from('tasksche.exe'),
          Buffer.from('.WNCRYT'),
          Buffer.from('@WanaDecryptor@'),
          Buffer.from('Wana Decrypt0r'),
        ],
        minMatches: 2
      },
      {
        name: 'Ransomware.Ryuk',
        severity: 'critical',
        category: 'ransomware',
        strings: [
          Buffer.from('RyukReadMe'),
          Buffer.from('.RYK'),
          Buffer.from('UNIQUE_ID'),
        ],
        minMatches: 2
      },
      {
        name: 'Ransomware.LockBit',
        severity: 'critical',
        category: 'ransomware',
        strings: [
          Buffer.from('LockBit'),
          Buffer.from('.lockbit'),
          Buffer.from('Restore-My-Files'),
        ],
        minMatches: 2
      },
      {
        name: 'Ransomware.Conti',
        severity: 'critical',
        category: 'ransomware',
        strings: [
          Buffer.from('CONTI_README'),
          Buffer.from('.CONTI'),
        ],
        minMatches: 1
      },

      // Banking Trojans
      {
        name: 'Trojan.Emotet',
        severity: 'critical',
        category: 'trojan',
        strings: [
          Buffer.from('WScript.Shell'),
          Buffer.from('powershell.exe -enc'),
          Buffer.from('RegSvr32.exe'),
        ],
        hexStrings: ['558BEC83EC', '5589E583EC'],
        minMatches: 2
      },
      {
        name: 'Trojan.TrickBot',
        severity: 'critical',
        category: 'trojan',
        strings: [
          Buffer.from('pwgrab'),
          Buffer.from('systeminfo'),
          Buffer.from('mailsearcher'),
          Buffer.from('injectDll'),
        ],
        minMatches: 2
      },
      {
        name: 'Trojan.Qbot',
        severity: 'critical',
        category: 'trojan',
        strings: [
          Buffer.from('qbot'),
          Buffer.from('%programdata%'),
          Buffer.from('explorer.exe'),
        ],
        hexStrings: ['E8000000005D'],
        minMatches: 2
      },
      {
        name: 'Trojan.Dridex',
        severity: 'critical',
        category: 'trojan',
        strings: [
          Buffer.from('dridex'),
          Buffer.from('browser'),
          Buffer.from('inject'),
        ],
        minMatches: 2
      },

      // Info Stealers
      {
        name: 'Stealer.RedLine',
        severity: 'high',
        category: 'stealer',
        strings: [
          Buffer.from('RedLine'),
          Buffer.from('AuthToken'),
          Buffer.from('\\Local State'),
          Buffer.from('\\Login Data'),
        ],
        minMatches: 2
      },
      {
        name: 'Stealer.Raccoon',
        severity: 'high',
        category: 'stealer',
        strings: [
          Buffer.from('RC4'),
          Buffer.from('telegram'),
          Buffer.from('grabber'),
          Buffer.from('wallets'),
        ],
        minMatches: 2
      },
      {
        name: 'Stealer.Vidar',
        severity: 'high',
        category: 'stealer',
        strings: [
          Buffer.from('Vidar'),
          Buffer.from('profile'),
          Buffer.from('grabber'),
          Buffer.from('Information.txt'),
        ],
        minMatches: 2
      },
      {
        name: 'Stealer.AgentTesla',
        severity: 'high',
        category: 'stealer',
        strings: [
          Buffer.from('Agent Tesla'),
          Buffer.from('smtp'),
          Buffer.from('keylog'),
          Buffer.from('screenshot'),
        ],
        minMatches: 2
      },
      {
        name: 'Stealer.FormBook',
        severity: 'high',
        category: 'stealer',
        strings: [
          Buffer.from('FormBook'),
          Buffer.from('XLoader'),
          Buffer.from('grabber'),
        ],
        minMatches: 1
      },

      // RATs (Remote Access Trojans)
      {
        name: 'RAT.AsyncRAT',
        severity: 'high',
        category: 'rat',
        strings: [
          Buffer.from('AsyncClient'),
          Buffer.from('Pastebin.com'),
          Buffer.from('delay'),
          Buffer.from('Anti'),
        ],
        minMatches: 2
      },
      {
        name: 'RAT.njRAT',
        severity: 'high',
        category: 'rat',
        strings: [
          Buffer.from('njRAT'),
          Buffer.from('Bladabindi'),
          Buffer.from('reg_'),
          Buffer.from('cap_'),
        ],
        minMatches: 2
      },
      {
        name: 'RAT.Remcos',
        severity: 'high',
        category: 'rat',
        strings: [
          Buffer.from('Remcos'),
          Buffer.from('RemoteControl'),
          Buffer.from('Keylogger'),
        ],
        minMatches: 2
      },
      {
        name: 'RAT.NanoCore',
        severity: 'high',
        category: 'rat',
        strings: [
          Buffer.from('NanoCore'),
          Buffer.from('ClientPlugin'),
          Buffer.from('PipeServer'),
        ],
        minMatches: 2
      },

      // Backdoors
      {
        name: 'Backdoor.CobaltStrike',
        severity: 'critical',
        category: 'backdoor',
        strings: [
          Buffer.from('beacon'),
          Buffer.from('ReflectiveLoader'),
          Buffer.from('%c%c%c%c%c%c%c%c%cMSSE'),
        ],
        hexStrings: ['4D5A90000300000004000000'],
        minMatches: 1
      },
      {
        name: 'Backdoor.Metasploit',
        severity: 'high',
        category: 'backdoor',
        strings: [
          Buffer.from('meterpreter'),
          Buffer.from('stdapi'),
          Buffer.from('ReflectiveDllInjection'),
        ],
        minMatches: 1
      },

      // Cryptocurrency Miners
      {
        name: 'Miner.XMRig',
        severity: 'high',
        category: 'miner',
        strings: [
          Buffer.from('xmrig'),
          Buffer.from('donate-level'),
          Buffer.from('randomx'),
          Buffer.from('pool.minexmr.com'),
        ],
        minMatches: 2
      },
      {
        name: 'Miner.Generic',
        severity: 'medium',
        category: 'miner',
        strings: [
          Buffer.from('stratum+tcp'),
          Buffer.from('cryptonight'),
          Buffer.from('cpuminer'),
          Buffer.from('mining.pool'),
        ],
        minMatches: 2
      },

      // Credential Dumpers
      {
        name: 'Hacktool.Mimikatz',
        severity: 'critical',
        category: 'hacktool',
        strings: [
          Buffer.from('mimikatz'),
          Buffer.from('sekurlsa'),
          Buffer.from('lsadump'),
          Buffer.from('gentilkiwi'),
        ],
        minMatches: 2
      },
      {
        name: 'Hacktool.LaZagne',
        severity: 'high',
        category: 'hacktool',
        strings: [
          Buffer.from('lazagne'),
          Buffer.from('Password'),
          Buffer.from('CredentialManager'),
        ],
        minMatches: 2
      },

      // Keyloggers
      {
        name: 'Keylogger.Generic',
        severity: 'high',
        category: 'keylogger',
        strings: [
          Buffer.from('GetAsyncKeyState'),
          Buffer.from('[ENTER]'),
          Buffer.from('[BACKSPACE]'),
          Buffer.from('keylog'),
        ],
        minMatches: 2
      },

      // Downloaders/Droppers
      {
        name: 'Downloader.Generic',
        severity: 'medium',
        category: 'downloader',
        strings: [
          Buffer.from('URLDownloadToFile'),
          Buffer.from('InternetOpenUrl'),
          Buffer.from('powershell'),
          Buffer.from('Invoke-WebRequest'),
        ],
        minMatches: 2
      },

      // Packed/Obfuscated
      {
        name: 'Packer.UPX',
        severity: 'low',
        category: 'packer',
        strings: [
          Buffer.from('UPX0'),
          Buffer.from('UPX1'),
          Buffer.from('UPX!'),
        ],
        minMatches: 2
      },
      {
        name: 'Packer.Themida',
        severity: 'medium',
        category: 'packer',
        strings: [
          Buffer.from('.themida'),
          Buffer.from('Oreans'),
        ],
        minMatches: 1
      },
      {
        name: 'Packer.VMProtect',
        severity: 'medium',
        category: 'packer',
        strings: [
          Buffer.from('VMProtect'),
          Buffer.from('.vmp0'),
          Buffer.from('.vmp1'),
        ],
        minMatches: 1
      },

      // Web Shells
      {
        name: 'WebShell.Generic',
        severity: 'high',
        category: 'webshell',
        strings: [
          Buffer.from('<?php eval('),
          Buffer.from('base64_decode'),
          Buffer.from('system($_'),
          Buffer.from('passthru'),
        ],
        minMatches: 2
      },

      // Rootkits
      {
        name: 'Rootkit.Generic',
        severity: 'critical',
        category: 'rootkit',
        strings: [
          Buffer.from('ZwSetSystemInformation'),
          Buffer.from('KeServiceDescriptorTable'),
          Buffer.from('SSDT'),
          Buffer.from('\\Device\\'),
        ],
        minMatches: 2
      }
    ];
  }

  /**
   * Scan buffer against all signatures
   */
  scanBuffer(buffer) {
    const matches = [];
    const hexBuffer = buffer.toString('hex');

    for (const signature of this.signatures) {
      let matchCount = 0;
      const matchedStrings = [];

      // Check string matches
      if (signature.strings) {
        for (const searchString of signature.strings) {
          if (buffer.includes(searchString)) {
            matchCount++;
            matchedStrings.push(searchString.toString());
          }
        }
      }

      // Check hex string matches
      if (signature.hexStrings) {
        for (const hexString of signature.hexStrings) {
          if (hexBuffer.includes(hexString.toLowerCase())) {
            matchCount++;
            matchedStrings.push(`hex:${hexString}`);
          }
        }
      }

      // If minimum matches met, add to results
      if (matchCount >= signature.minMatches) {
        matches.push({
          name: signature.name,
          severity: signature.severity,
          category: signature.category,
          matchCount,
          matchedStrings: matchedStrings.slice(0, 5) // Limit to 5 for display
        });
      }
    }

    return matches;
  }

  /**
   * Scan file
   */
  async scanFile(filePath) {
    const fs = require('fs').promises;
    const buffer = await fs.readFile(filePath);
    return this.scanBuffer(buffer);
  }

  /**
   * Get threat score from matches
   */
  getThreatScore(matches) {
    let score = 0;

    matches.forEach(match => {
      switch (match.severity) {
        case 'critical':
          score += 50;
          break;
        case 'high':
          score += 30;
          break;
        case 'medium':
          score += 15;
          break;
        case 'low':
          score += 5;
          break;
      }
    });

    return Math.min(100, score);
  }
}

module.exports = SignatureEngine;
