#!/usr/bin/env python3
"""
Fix Emoji Encoding Issues - Remove all emojis from source files
Fixes UnicodeEncodeError: 'charmap' codec can't encode characters
"""

import os
import re
from pathlib import Path

def remove_emojis(text):
    """Remove all emojis and unicode symbols from text"""
    
    # Comprehensive emoji and unicode symbol patterns
    emoji_patterns = [
        # Emoticons
        r'[\U0001F600-\U0001F64F]',
        # Symbols & Pictographs  
        r'[\U0001F300-\U0001F5FF]',
        # Transport & Map symbols
        r'[\U0001F680-\U0001F6FF]',
        # Flags
        r'[\U0001F1E0-\U0001F1FF]',
        # Miscellaneous Symbols
        r'[\U0001F900-\U0001F9FF]',
        # Supplemental Symbols
        r'[\U0001FA70-\U0001FAFF]',
        # Dingbats
        r'[\U00002700-\U000027BF]',
        # Miscellaneous Technical
        r'[\U00002300-\U000023FF]',
        # Enclosed Characters
        r'[\U000024C2-\U0001F251]',
        # Geometric Shapes
        r'[\U000025A0-\U000025FF]',
        # Miscellaneous Symbols and Arrows
        r'[\U00002B00-\U00002BFF]',
        # CJK Symbols and Punctuation
        r'[\U00003000-\U0000303F]',
        # Latin Extended Additional
        r'[\U0001E00-\U0001EFF]',
        # General Punctuation
        r'[\U00002000-\U0000206F]',
        # Currency Symbols
        r'[\U000020A0-\U000020CF]',
        # Letterlike Symbols
        r'[\U00002100-\U0000214F]',
        # Number Forms
        r'[\U00002150-\U0000218F]',
        # Arrows
        r'[\U00002190-\U000021FF]',
        # Mathematical Operators
        r'[\U00002200-\U000022FF]',
        # Box Drawing
        r'[\U00002500-\U0000257F]',
        # Block Elements
        r'[\U00002580-\U0000259F]',
        # Geometric Shapes
        r'[\U000025A0-\U000025FF]',
        # Miscellaneous Symbols
        r'[\U00002600-\U000026FF]',
        # Dingbats
        r'[\U00002700-\U000027BF]',
        # CJK Symbols and Punctuation
        r'[\U00003000-\U0000303F]',
        # Hiragana
        r'[\U00003040-\U0000309F]',
        # Katakana
        r'[\U000030A0-\U000030FF]',
        # Bopomofo
        r'[\U00003100-\U0000312F]',
        # Hangul Compatibility Jamo
        r'[\U00003130-\U0000318F]',
        # Kanbun
        r'[\U00003190-\U0000319F]',
        # Bopomofo Extended
        r'[\U000031A0-\U000031BF]',
        # CJK Strokes
        r'[\U000031C0-\U000031EF]',
        # Katakana Phonetic Extensions
        r'[\U000031F0-\U000031FF]',
        # Enclosed CJK Letters and Months
        r'[\U00003200-\U000032FF]',
        # CJK Compatibility
        r'[\U00003300-\U000033FF]',
        # CJK Unified Ideographs Extension A
        r'[\U00003400-\U00004DBF]',
        # Yijing Hexagram Symbols
        r'[\U00004DC0-\U00004DFF]',
        # CJK Unified Ideographs
        r'[\U00004E00-\U00009FFF]',
        # Yi Syllables
        r'[\U0000A000-\U0000A48F]',
        # Yi Radicals
        r'[\U0000A490-\U0000A4CF]',
        # Lisu
        r'[\U0000A4D0-\U0000A4FF]',
        # Vai
        r'[\U0000A500-\U0000A63F]',
        # Cyrillic Extended-B
        r'[\U0000A640-\U0000A69F]',
        # Bamum
        r'[\U0000A6A0-\U0000A6FF]',
        # Modifier Tone Letters
        r'[\U0000A700-\U0000A71F]',
        # Latin Extended-D
        r'[\U0000A720-\U0000A7FF]',
        # Syloti Nagri
        r'[\U0000A800-\U0000A82F]',
        # Common Indic Number Forms
        r'[\U0000A830-\U0000A83F]',
        # Phags-pa
        r'[\U0000A840-\U0000A87F]',
        # Saurashtra
        r'[\U0000A880-\U0000A8DF]',
        # Devanagari Extended
        r'[\U0000A8E0-\U0000A8FF]',
        # Kayah Li
        r'[\U0000A900-\U0000A92F]',
        # Rejang
        r'[\U0000A930-\U0000A95F]',
        # Hangul Jamo Extended-A
        r'[\U0000A960-\U0000A97F]',
        # Javanese
        r'[\U0000A980-\U0000A9DF]',
        # Myanmar Extended-B
        r'[\U0000A9E0-\U0000A9FF]',
        # Cham
        r'[\U0000AA00-\U0000AA5F]',
        # Myanmar Extended-A
        r'[\U0000AA60-\U0000AA7F]',
        # Tai Viet
        r'[\U0000AA80-\U0000AADF]',
        # Meetei Mayek Extensions
        r'[\U0000AAE0-\U0000AAFF]',
        # Ethiopic Extended-A
        r'[\U0000AB00-\U0000AB2F]',
        # Latin Extended-E
        r'[\U0000AB30-\U0000AB6F]',
        # Cherokee Supplement
        r'[\U0000AB70-\U0000ABBF]',
        # Meetei Mayek
        r'[\U0000ABC0-\U0000ABFF]',
        # Hangul Syllables
        r'[\U0000AC00-\U0000D7AF]',
        # Hangul Jamo Extended-B
        r'[\U0000D7B0-\U0000D7FF]',
        # High Surrogates
        r'[\U0000D800-\U0000DB7F]',
        # High Private Use Surrogates
        r'[\U0000DB80-\U0000DBFF]',
        # Low Surrogates
        r'[\U0000DC00-\U0000DFFF]',
        # Private Use Area
        r'[\U0000E000-\U0000F8FF]',
        # CJK Compatibility Ideographs
        r'[\U0000F900-\U0000FAFF]',
        # Alphabetic Presentation Forms
        r'[\U0000FB00-\U0000FB4F]',
        # Arabic Presentation Forms-A
        r'[\U0000FB50-\U0000FDFF]',
        # Variation Selectors
        r'[\U0000FE00-\U0000FE0F]',
        # Vertical Forms
        r'[\U0000FE10-\U0000FE1F]',
        # Combining Half Marks
        r'[\U0000FE20-\U0000FE2F]',
        # CJK Compatibility Forms
        r'[\U0000FE30-\U0000FE4F]',
        # Small Form Variants
        r'[\U0000FE50-\U0000FE6F]',
        # Arabic Presentation Forms-B
        r'[\U0000FE70-\U0000FEFF]',
        # Halfwidth and Fullwidth Forms
        r'[\U0000FF00-\U0000FFEF]',
        # Specials
        r'[\U0000FFF0-\U0000FFFF]',
        # Linear B Syllable B038
        r'[\U00010000-\U0001007F]',
        # Linear B Ideograms
        r'[\U00010080-\U000100FF]',
        # Aegean Numbers
        r'[\U00010100-\U0001013F]',
        # Ancient Greek Numbers
        r'[\U00010140-\U0001018F]',
        # Ancient Symbols
        r'[\U00010190-\U000101CF]',
        # Phaistos Disc
        r'[\U000101D0-\U000101FF]',
        # Lycian
        r'[\U00010280-\U0001029F]',
        # Carian
        r'[\U000102A0-\U000102DF]',
        # Coptic Epact Numbers
        r'[\U000102E0-\U000102FF]',
        # Old Italic
        r'[\U00010300-\U0001032F]',
        # Gothic
        r'[\U00010330-\U0001034F]',
        # Old Permic
        r'[\U00010350-\U0001037F]',
        # Ugaritic
        r'[\U00010380-\U0001039F]',
        # Old Persian
        r'[\U000103A0-\U000103DF]',
        # Deseret
        r'[\U00010400-\U0001044F]',
        # Shavian
        r'[\U00010450-\U0001047F]',
        # Osmanya
        r'[\U00010480-\U000104AF]',
        # Osage
        r'[\U000104B0-\U000104FF]',
        # Elbasan
        r'[\U00010500-\U0001052F]',
        # Caucasian Albanian
        r'[\U00010530-\U0001056F]',
        # Linear A
        r'[\U00010600-\U0001077F]',
        # Cypriot Syllabary
        r'[\U00010800-\U0001083F]',
        # Imperial Aramaic
        r'[\U00010840-\U0001085F]',
        # Palmyrene
        r'[\U00010860-\U0001087F]',
        # Nabataean
        r'[\U00010880-\U000108AF]',
        # Hatran
        r'[\U000108E0-\U000108FF]',
        # Phoenician
        r'[\U00010900-\U0001091F]',
        # Lydian
        r'[\U00010920-\U0001093F]',
        # Meroitic Hieroglyphs
        r'[\U00010980-\U0001099F]',
        # Meroitic Cursive
        r'[\U000109A0-\U000109FF]',
        # Kharoshthi
        r'[\U00010A00-\U00010A5F]',
        # Old South Arabian
        r'[\U00010A60-\U00010A7F]',
        # Old North Arabian
        r'[\U00010A80-\U00010A9F]',
        # Manichaean
        r'[\U00010AC0-\U00010AFF]',
        # Avestan
        r'[\U00010B00-\U00010B3F]',
        # Inscriptional Parthian
        r'[\U00010B40-\U00010B5F]',
        # Inscriptional Pahlavi
        r'[\U00010B60-\U00010B7F]',
        # Psalter Pahlavi
        r'[\U00010B80-\U00010BAF]',
        # Old Turkic
        r'[\U00010C00-\U00010C4F]',
        # Old Hungarian
        r'[\U00010C80-\U00010CFF]',
        # Hanifi Rohingya
        r'[\U00010D00-\U00010D3F]',
        # Rumi Numeral Symbols
        r'[\U00010E60-\U00010E7F]',
        # Yezidi
        r'[\U00010E80-\U00010EBF]',
        # Arabic Extended-C
        r'[\U00010EC0-\U00010EFF]',
        # Old Sogdian
        r'[\U00010F00-\U00010F2F]',
        # Sogdian
        r'[\U00010F30-\U00010F6F]',
        # Chorasmian
        r'[\U00010F70-\U00010FAF]',
        # Elymaic
        r'[\U00010FE0-\U00010FFF]',
        # Brahmi
        r'[\U00011000-\U0001107F]',
        # Kaithi
        r'[\U00011080-\U000110CF]',
        # Sora Sompeng
        r'[\U000110D0-\U000110FF]',
        # Chakma
        r'[\U00011100-\U0001114F]',
        # Mahajani
        r'[\U00011150-\U0001117F]',
        # Sharada
        r'[\U00011180-\U000111DF]',
        # Sinhala Archaic Numbers
        r'[\U000111E0-\U000111FF]',
        # Khojki
        r'[\U00011200-\U0001124F]',
        # Multani
        r'[\U00011280-\U000112AF]',
        # Khudawadi
        r'[\U000112B0-\U000112FF]',
        # Grantha
        r'[\U00011300-\U0001137F]',
        # Newa
        r'[\U00011400-\U0001147F]',
        # Tirhuta
        r'[\U00011480-\U000114DF]',
        # Siddham
        r'[\U00011580-\U000115FF]',
        # Modi
        r'[\U00011600-\U0001165F]',
        # Mongolian Supplement
        r'[\U00011660-\U0001167F]',
        # Takri
        r'[\U00011680-\U000116CF]',
        # Ahom
        r'[\U00011700-\U0001173F]',
        # Dogra
        r'[\U00011800-\U0001184F]',
        # Warang Citi
        r'[\U000118A0-\U000118FF]',
        # Dives Akuru
        r'[\U00011900-\U0001195F]',
        # Nandinagari
        r'[\U000119A0-\U000119FF]',
        # Zanabazar Square
        r'[\U00011A00-\U00011A4F]',
        # Soyombo
        r'[\U00011A50-\U00011AAF]',
        # Pau Cin Hau
        r'[\U00011AC0-\U00011AFF]',
        # Bhaiksuki
        r'[\U00011C00-\U00011C6F]',
        # Marchen
        r'[\U00011C70-\U00011CBF]',
        # Masaram Gondi
        r'[\U00011D00-\U00011D5F]',
        # Gunjala Gondi
        r'[\U00011D60-\U00011DAF]',
        # Makasar
        r'[\U00011EE0-\U00011EFF]',
        # Lisu Supplement
        r'[\U00011FB0-\U00011FBF]',
        # Tamil Supplement
        r'[\U00011FC0-\U00011FFF]',
        # Cuneiform
        r'[\U00012000-\U000123FF]',
        # Cuneiform Numbers and Punctuation
        r'[\U00012400-\U0001247F]',
        # Early Dynastic Cuneiform
        r'[\U00012480-\U0001254F]',
        # Cypro-Minoan
        r'[\U00012F90-\U00012FFF]',
        # Egyptian Hieroglyphs
        r'[\U00013000-\U0001342F]',
        # Egyptian Hieroglyph Format Controls
        r'[\U00013430-\U0001343F]',
        # Anatolian Hieroglyphs
        r'[\U00014400-\U0001467F]',
        # Bamum Supplement
        r'[\U00016A40-\U00016A6F]',
        # Mro
        r'[\U00016A70-\U00016ACF]',
        # Tangsa
        r'[\U00016AD0-\U00016AFF]',
        # Bassa Vah
        r'[\U00016B00-\U00016B8F]',
        # Pahawh Hmong
        r'[\U00016B90-\U00016BFF]',
        # Medefaidrin
        r'[\U00016E40-\U00016E9F]',
        # Miao
        r'[\U00016F00-\U00016F9F]',
        # Ideographic Symbols and Punctuation
        r'[\U00016FE0-\U00016FFF]',
        # Tangut
        r'[\U00017000-\U000187FF]',
        # Tangut Components
        r'[\U00018800-\U00018AFF]',
        # Khitan Small Script
        r'[\U00018B00-\U00018CFF]',
        # Tangut Supplement
        r'[\U00018D00-\U00018D7F]',
        # Kana Extended-B
        r'[\U0001AFF0-\U0001AFFF]',
        # Kana Supplement
        r'[\U0001B000-\U0001B0FF]',
        # Kana Extended-A
        r'[\U0001B100-\U0001B12F]',
        # Small Kana Extension
        r'[\U0001B130-\U0001B16F]',
        # Nushu
        r'[\U0001B170-\U0001B2FF]',
        # Duployan
        r'[\U0001BC00-\U0001BC9F]',
        # Shorthand Format Controls
        r'[\U0001BCA0-\U0001BCAF]',
        # Znamenny Musical Notation
        r'[\U0001CF00-\U0001CFCF]',
        # Byzantine Musical Symbols
        r'[\U0001D000-\U0001D0FF]',
        # Musical Symbols
        r'[\U0001D100-\U0001D1FF]',
        # Ancient Greek Musical Notation
        r'[\U0001D200-\U0001D24F]',
        # Mayan Numerals
        r'[\U0001D2E0-\U0001D2FF]',
        # Tai Xuan Jing Symbols
        r'[\U0001D300-\U0001D35F]',
        # Counting Rod Numerals
        r'[\U0001D360-\U0001D37F]',
        # Mathematical Alphanumeric Symbols
        r'[\U0001D400-\U0001D7FF]',
        # Sutton SignWriting
        r'[\U0001D800-\U0001DAAF]',
        # Glagolitic Supplement
        r'[\U0001E000-\U0001E02F]',
        # Nyiakeng Puachue Hmong
        r'[\U0001E100-\U0001E14F]',
        # Wancho
        r'[\U0001E2C0-\U0001E2FF]',
        # Mende Kikakui
        r'[\U0001E800-\U0001E8DF]',
        # Adlam
        r'[\U0001E900-\U0001E95F]',
        # Indic Siyaq Numbers
        r'[\U0001EC70-\U0001ECBF]',
        # Ottoman Siyaq Numbers
        r'[\U0001ED00-\U0001ED4F]',
        # Arabic Mathematical Alphabetic Symbols
        r'[\U0001EE00-\U0001EEFF]',
        # Mahjong Tiles
        r'[\U0001F000-\U0001F02F]',
        # Domino Tiles
        r'[\U0001F030-\U0001F09F]',
        # Playing Cards
        r'[\U0001F0A0-\U0001F0FF]',
        # Enclosed Alphanumeric Supplement
        r'[\U0001F100-\U0001F1FF]',
        # Enclosed Ideographic Supplement
        r'[\U0001F200-\U0001F2FF]',
        # Miscellaneous Symbols and Pictographs
        r'[\U0001F300-\U0001F5FF]',
        # Emoticons
        r'[\U0001F600-\U0001F64F]',
        # Ornamental Dingbats
        r'[\U0001F650-\U0001F67F]',
        # Transport and Map Symbols
        r'[\U0001F680-\U0001F6FF]',
        # Alchemical Symbols
        r'[\U0001F700-\U0001F77F]',
        # Geometric Shapes Extended
        r'[\U0001F780-\U0001F7FF]',
        # Supplemental Arrows-C
        r'[\U0001F800-\U0001F8FF]',
        # Supplemental Symbols and Pictographs
        r'[\U0001F900-\U0001F9FF]',
        # Chess Symbols
        r'[\U0001FA00-\U0001FA6F]',
        # Symbols and Pictographs Extended-A
        r'[\U0001FA70-\U0001FAFF]',
        # Symbols for Legacy Computing
        r'[\U0001FB00-\U0001FBFF]',
        # CJK Unified Ideographs Extension B
        r'[\U00020000-\U0002A6DF]',
        # CJK Unified Ideographs Extension C
        r'[\U0002A700-\U0002B73F]',
        # CJK Unified Ideographs Extension D
        r'[\U0002B740-\U0002B81F]',
        # CJK Unified Ideographs Extension E
        r'[\U0002B820-\U0002CEAF]',
        # CJK Unified Ideographs Extension F
        r'[\U0002CEB0-\U0002EBEF]',
        # CJK Compatibility Ideographs Supplement
        r'[\U0002F800-\U0002FA1F]',
        # CJK Unified Ideographs Extension G
        r'[\U00030000-\U0003134F]',
        # Tags
        r'[\U000E0000-\U000E007F]',
        # Variation Selectors Supplement
        r'[\U000E0100-\U000E01EF]',
        # Supplementary Private Use Area-A
        r'[\U000F0000-\U000FFFFF]',
        # Supplementary Private Use Area-B
        r'[\U00100000-\U0010FFFF]'
    ]
    
    # Combine all patterns
    combined_pattern = '|'.join(emoji_patterns)
    
    # Remove all matches
    cleaned_text = re.sub(combined_pattern, '', text)
    
    # Also remove any remaining high Unicode characters
    cleaned_text = re.sub(r'[^\x00-\x7F]', '', cleaned_text)
    
    return cleaned_text

def fix_file(file_path):
    """Fix emoji encoding in a single file"""
    try:
        # Read file with different encodings
        content = None
        for encoding in ['utf-8', 'utf-8-sig', 'latin-1', 'cp1252']:
            try:
                with open(file_path, 'r', encoding=encoding) as f:
                    content = f.read()
                print(f"Read {file_path} with {encoding} encoding")
                break
            except UnicodeDecodeError:
                continue
        
        if content is None:
            print(f"ERROR: Could not read {file_path}")
            return False
        
        original_size = len(content)
        cleaned_content = remove_emojis(content)
        new_size = len(cleaned_content)
        
        if original_size != new_size:
            # Write back with UTF-8 encoding
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(cleaned_content)
            print(f"FIXED: {file_path} ({original_size - new_size} chars removed)")
            return True
        else:
            print(f"OK: {file_path} (no emojis found)")
            return False
            
    except Exception as e:
        print(f"ERROR: {file_path} - {e}")
        return False

def main():
    """Fix all Python files in the directory"""
    print("=" * 60)
    print("EMOJI ENCODING FIXER")
    print("Fixing UnicodeEncodeError: 'charmap' codec issues")
    print("=" * 60)
    
    # Files to fix
    files_to_fix = [
        "safe_hybrid_ide.py",
        "eon-compiler-gui.py", 
        "proper_exe_compiler.py",
        "enhanced_code_generator.py",
        "unified_ide_launcher.py"
    ]
    
    fixed_count = 0
    total_count = 0
    
    for file_path in files_to_fix:
        if os.path.exists(file_path):
            total_count += 1
            if fix_file(file_path):
                fixed_count += 1
        else:
            print(f"SKIP: {file_path} (not found)")
    
    print("=" * 60)
    print(f"FIX COMPLETE:")
    print(f"Files processed: {total_count}")
    print(f"Files fixed: {fixed_count}")
    print(f"Files unchanged: {total_count - fixed_count}")
    print("=" * 60)
    
    if fixed_count > 0:
        print("✅ All emoji encoding issues fixed!")
        print("🚀 Your IDE should now run without UnicodeEncodeError!")
    else:
        print("ℹ️  No emoji issues found - your files are clean!")

if __name__ == "__main__":
    main()
