$src = Get-Content "D:\rawrxd\Ship\RawrXD_Win32_IDE.cpp" -Raw
$p=0; $f=0
$tests = @(
    @('ID_FILE_OPENFOLDER defined', '#define\s+ID_FILE_OPENFOLDER'),
    @('IDM_FILE_OPENFOLDER alias', '#define\s+IDM_FILE_OPENFOLDER'),
    @('Open Folder menu item', 'Open &Folder'),
    @('case IDM_FILE_OPENFOLDER', 'case IDM_FILE_OPENFOLDER'),
    @('BrowseFolderDialog impl', 'std::wstring BrowseFolderDialog'),
    @('IFileOpenDialog COM', 'IFileOpenDialog'),
    @('PopulateFileTree impl', 'void PopulateFileTree'),
    @('FindFirstFileW enum', 'FindFirstFileW'),
    @('TVM_INSERTITEMW calls', 'TVM_INSERTITEMW'),
    @('GetTreeItemPath impl', 'std::wstring GetTreeItemPath'),
    @('WM_NOTIFY handler', 'case WM_NOTIFY'),
    @('TVN_ITEMEXPANDINGW lazy', 'TVN_ITEMEXPANDINGW'),
    @('NM_DBLCLK open file', 'NM_DBLCLK'),
    @('g_workspaceRoot assigned', 'g_workspaceRoot = folder'),
    @('Dirs sorted', 'std::sort\(dirs'),
    @('cChildren = 1 for dirs', 'cChildren = 1')
)
foreach ($t in $tests) {
    if ($src -match $t[1]) { Write-Host "PASS: $($t[0])"; $p++ } else { Write-Host "FAIL: $($t[0])"; $f++ }
}
Write-Host ""; Write-Host "File Tree: $p PASS / $f FAIL"
