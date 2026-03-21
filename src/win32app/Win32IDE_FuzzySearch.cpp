// ============================================================================
// Win32IDE_FuzzySearch.cpp — Tier 1 Cosmetic #4: Enhanced Fuzzy Search Utilities
// ============================================================================
// Provides class-level fuzzy search wrappers and fuzzy-highlight painting
// for the command palette. The core fzf-style algorithm is already implemented
// in Win32IDE_Commands.cpp. This file adds:
//   - Public fuzzyMatchScore wrapper (calls the static implementation)
//   - Fuzzy filter with MRU awareness
//   - Highlighted match painting for owner-draw listbox items
//
// Pattern:  No exceptions, PatchResult-compatible
// Threading: UI thread only
// ============================================================================

#include "Win32IDE.h"
#include <algorithm>

// Colors for fuzzy match highlighting
static const COLORREF FUZZY_MATCH_COLOR    = RGB(255, 200, 60);    // yellow highlight
static const COLORREF FUZZY_NORMAL_COLOR   = RGB(204, 204, 204);   // normal text
static const COLORREF FUZZY_BG             = RGB(37, 37, 38);
static const COLORREF FUZZY_SELECTED_BG    = RGB(4, 57, 94);       // VS Code selection blue

// ============================================================================
// CLASS-LEVEL FUZZY MATCH SCORE
// ============================================================================

int Win32IDE::fuzzyMatchScore(const std::string& pattern, const std::string& candidate,
                              std::vector<int>* matchPositions)
{
    // Case-insensitive character-skip fuzzy matching (fzf-style)
    if (pattern.empty()) return 0;

    std::string lp, lc;
    lp.resize(pattern.size());
    lc.resize(candidate.size());
    std::transform(pattern.begin(), pattern.end(), lp.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    std::transform(candidate.begin(), candidate.end(), lc.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });

    int qi = 0;
    int prevMatchIdx = -1;
    bool afterSeparator = true;
    int score = 0;
    std::vector<int> positions;

    for (int ti = 0; ti < (int)lc.size() && qi < (int)lp.size(); ti++) {
        if (lc[ti] == lp[qi]) {
            positions.push_back(ti);

            // Scoring bonuses (fzf-style)
            if (afterSeparator) {
                score += 10;  // word boundary match
            } else if (prevMatchIdx >= 0 && ti == prevMatchIdx + 1) {
                score += 5;   // consecutive character
            } else {
                score += 1;   // gap match
            }

            // Exact case bonus
            if (qi < (int)pattern.size() && ti < (int)candidate.size() &&
                pattern[qi] == candidate[ti]) {
                score += 2;
            }

            prevMatchIdx = ti;
            qi++;
        }

        afterSeparator = (lc[ti] == ' ' || lc[ti] == ':' || lc[ti] == '/' ||
                          lc[ti] == '\\' || lc[ti] == '_' || lc[ti] == '-');
    }

    bool matched = (qi == (int)lp.size());
    if (!matched) return -1; // no match

    // Bonus for shorter targets (tighter match)
    score += (std::max)(0, 50 - (int)candidate.size());

    // Penalize for match spread
    if (!positions.empty()) {
        int spread = positions.back() - positions.front();
        score -= spread / 2;
    }

    if (matchPositions) {
        *matchPositions = positions;
    }

    return score;
}

// ============================================================================
// FUZZY FILTER COMMAND PALETTE — Wrapper that delegates to existing implementation
// ============================================================================

void Win32IDE::fuzzyFilterCommandPalette(const std::string& query)
{
    // Delegate to the existing filterCommandPalette which already
    // uses fuzzyMatchScore with MRU boost and category prefix filtering
    filterCommandPalette(query);
}

// ============================================================================
// PAINT FUZZY HIGHLIGHTS — Owner-draw rendering for command palette items
// ============================================================================

void Win32IDE::paintFuzzyHighlights(HDC hdc, RECT itemRect, const std::string& text,
                                     const std::vector<int>& matchPositions)
{
    // Render text with highlighted match characters (yellow on matched chars)
    SetBkMode(hdc, TRANSPARENT);

    // Create a set of matched positions for O(1) lookup
    std::vector<bool> isMatch(text.size(), false);
    for (int pos : matchPositions) {
        if (pos >= 0 && pos < (int)text.size()) {
            isMatch[pos] = true;
        }
    }

    int xPos = itemRect.left + 4;
    int yCenter = (itemRect.top + itemRect.bottom) / 2;

    // Measure character widths using current font
    SIZE charSize;
    GetTextExtentPoint32A(hdc, "M", 1, &charSize);
    int charHeight = charSize.cy;
    int yText = yCenter - charHeight / 2;

    for (size_t i = 0; i < text.size(); i++) {
        // Set color based on whether this char is a fuzzy match
        if (isMatch[i]) {
            SetTextColor(hdc, FUZZY_MATCH_COLOR);

            // Draw subtle underline for matched character
            RECT underline = { xPos, yText + charHeight, xPos + 0, yText + charHeight + 1 };
            // Will be sized below after measuring char
        } else {
            SetTextColor(hdc, FUZZY_NORMAL_COLOR);
        }

        // Draw single character
        char ch[2] = { text[i], '\0' };
        SIZE chSize;
        GetTextExtentPoint32A(hdc, ch, 1, &chSize);

        TextOutA(hdc, xPos, yText, ch, 1);

        // Draw underline for matched characters
        if (isMatch[i]) {
            HPEN hlPen = CreatePen(PS_SOLID, 1, FUZZY_MATCH_COLOR);
            HPEN oldPen = (HPEN)SelectObject(hdc, hlPen);
            MoveToEx(hdc, xPos, yText + charHeight, nullptr);
            LineTo(hdc, xPos + chSize.cx, yText + charHeight);
            SelectObject(hdc, oldPen);
            DeleteObject(hlPen);
        }

        xPos += chSize.cx;

        // Stop if we exceed the rect
        if (xPos > itemRect.right - 8) break;
    }
}
