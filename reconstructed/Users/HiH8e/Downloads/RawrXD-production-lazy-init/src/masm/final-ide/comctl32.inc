IFNDEF COMCTL32_INC
COMCTL32_INC EQU 1

extern InitCommonControlsEx : proc

; Tab Control
WC_TABCONTROLW EQU <'S',0,'y',0,'s',0,'T',0,'a',0,'b',0,'C',0,'o',0,'n',0,'t',0,'r',0,'o',0,'l',0,'3',0,'2',0,0,0>
TCM_INSERTITEMW EQU 1307h
TCM_SETCURSEL EQU 130Ch
TCM_GETCURSEL EQU 130Bh

; List View
WC_LISTVIEWW EQU <'S',0,'y',0,'s',0,'L',0,'i',0,'s',0,'t',0,'V',0,'i',0,'e',0,'w',0,'3',0,'2',0,0,0>
LVM_INSERTCOLUMNW EQU 1061h
LVM_INSERTITEMW EQU 104Dh
LVM_SETITEMTEXTW EQU 1074h

ENDIF
