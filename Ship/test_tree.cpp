#include <windows.h>
#include <stdio.h>
#include <commctrl.h>

#define WM_COMMAND 0x0111
#define ID_FILE_OPENFOLDER 1007
#define TVM_GETCOUNT 0x1105
#define TVM_GETNEXTITEM 0x110A
#define TVM_GETITEMW 0x113E
#define TVGN_ROOT 0
#define TVGN_CHILD 4
#define TVGN_NEXT 1
#define TVIF_TEXT 0x0001

int main() {
    HWND hwnd = FindWindowW(NULL, L"RawrXD IDE");
    if (!hwnd) { printf("IDE not found\n"); return 1; }
    printf("Main: %p\n\n", hwnd);

    // Find TreeView
    HWND tree = FindWindowExW(hwnd, NULL, L"SysTreeView32", NULL);
    if (!tree) { printf("TreeView not found\n"); return 1; }
    
    int items = (int)SendMessageW(tree, TVM_GETCOUNT, 0, 0);
    printf("Before: TreeView items=%d\n", items);

    // Send WM_COMMAND with IDM_FILE_OPENFOLDER
    // This will open the folder dialog - user needs to pick a folder
    printf("\nSending Open Folder command...\n");
    printf("(A folder picker dialog should appear - select D:\\rawrxd\\Ship)\n\n");
    PostMessageW(hwnd, WM_COMMAND, ID_FILE_OPENFOLDER, 0);
    
    // Wait for user to pick folder
    printf("Waiting 15 seconds for folder selection...\n");
    Sleep(15000);
    
    items = (int)SendMessageW(tree, TVM_GETCOUNT, 0, 0);
    printf("\nAfter: TreeView items=%d\n", items);
    
    if (items > 0) {
        printf("SUCCESS: File tree populated with %d items!\n", items);
        
        // Read root item text
        HTREEITEM hRoot = (HTREEITEM)SendMessageW(tree, TVM_GETNEXTITEM, TVGN_ROOT, 0);
        if (hRoot) {
            // Can't read text cross-process easily, but we can count children
            HTREEITEM hChild = (HTREEITEM)SendMessageW(tree, TVM_GETNEXTITEM, TVGN_CHILD, (LPARAM)hRoot);
            int childCount = 0;
            while (hChild) {
                childCount++;
                hChild = (HTREEITEM)SendMessageW(tree, TVM_GETNEXTITEM, TVGN_NEXT, (LPARAM)hChild);
            }
            printf("Root has %d children (dirs+files)\n", childCount);
        }
    } else {
        printf("FAIL: Tree still empty (did you select a folder?)\n");
    }
    
    // Check window title changed
    WCHAR title[512]; GetWindowTextW(hwnd, title, 512);
    printf("Window title: %ls\n", title);
    
    return 0;
}
