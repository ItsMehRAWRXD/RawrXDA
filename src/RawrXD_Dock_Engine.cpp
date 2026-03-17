#include <windows.h>
#include "include/rawrxd_dock_manager.h"

#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

/* 
 * RawrXD-style recursive layout engine for the Sovereign Docking Manager.
 * NO external UI framework. Pure x64 logic.
 */

void RawrMeasureNode(DOCK_NODE* node) {
    if (!node) return;

    // Reset constraints initially
    node->minW = 0;
    node->minH = 0;

    if (node->kind == DNK_LEAF) {
        // LEAF: panel min size (e.g., from PANEL_DESC)
        // Hardcoded for now, or from PANEL registry
        node->minW = 50; 
        node->minH = 50;
    } else if (node->kind == DNK_TABS) {
        // TABS: max of active children or all children min size
        // For simplicity: active child + tab strip height (24px)
        node->minW = 80;
        node->minH = 50 + 24; 
    } else if (node->kind == DNK_SPLIT) {
        // SPLIT: walk children to compute aggregate min size
        DOCK_NODE* childA = node->firstChild;
        DOCK_NODE* childB = (childA) ? childA->nextSibling : NULL;

        if (childA) RawrMeasureNode(childA);
        if (childB) RawrMeasureNode(childB);

        if (node->u.split.axis == AXIS_HORZ) {
            // Stacked vertically: sum heights, max width
            node->minW = max(childA ? childA->minW : 0, childB ? childB->minW : 0);
            node->minH = (childA ? childA->minH : 0) + (childB ? childB->minH : 0) + node->u.split.splitSize;
        } else if (node->u.split.axis == AXIS_VERT) {
            // Side-by-side: sum widths, max height
            node->minW = (childA ? childA->minW : 0) + (childB ? childB->minW : 0) + node->u.split.splitSize;
            node->minH = max(childA ? childA->minH : 0, childB ? childB->minH : 0);
        }
    }
}

void RawrArrangeNode(DOCK_NODE* node, RECT targetRect) {
    if (!node) return;

    node->rcBounds = targetRect;

    if (node->kind == DNK_LEAF) {
        node->rcContent = targetRect;
        // LEAF logic: will be applied to the panel's HWND later.
    } else if (node->kind == DNK_TABS) {
        // Tab strip at top (24px), content below
        node->rcTabStrip = targetRect;
        node->rcTabStrip.bottom = node->rcTabStrip.top + 24;

        node->rcContent = targetRect;
        node->rcContent.top = node->rcTabStrip.bottom;
    } else if (node->kind == DNK_SPLIT) {
        DOCK_NODE* childA = node->firstChild;
        DOCK_NODE* childB = (childA) ? childA->nextSibling : NULL;

        if (!childA || !childB) return; // Malformed split tree

        int splitPos = node->u.split.splitPos;
        int sSize = node->u.split.splitSize;

        RECT rcA = targetRect;
        RECT rcB = targetRect;
        RECT rcS = targetRect;

        if (node->u.split.axis == AXIS_VERT) {
            // VERT SPLIT: Side-by-side
            rcA.right = targetRect.left + splitPos;
            rcS.left = rcA.right;
            rcS.right = rcS.left + sSize;
            rcB.left = rcS.right;

            node->rcSplitter = rcS;
            RawrArrangeNode(childA, rcA);
            RawrArrangeNode(childB, rcB);
        } else if (node->u.split.axis == AXIS_HORZ) {
            // HORZ SPLIT: Stacked
            rcA.bottom = targetRect.top + splitPos;
            rcS.top = rcA.bottom;
            rcS.bottom = rcS.top + sSize;
            rcB.top = rcS.bottom;

            node->rcSplitter = rcS;
            RawrArrangeNode(childA, rcA);
            RawrArrangeNode(childB, rcB);
        }
    }
}

void RawrApplyLayout(UI_STATE* state) {
    if (!state || !state->pShellRoot) return;

    // Pass 1: Measure
    RawrMeasureNode(state->pShellRoot);

    // Pass 2: Arrange
    RawrArrangeNode(state->pShellRoot, state->rcDockHost);

    // Pass 3: Recompute viewport and move HWNDs recursively
    // This is handled by a separate tree-walker that calls SetWindowPos 
    // for nodes that have leaf.panelId or explicit HWNDs.
}
