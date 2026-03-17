	.file	"swarm_link_test.cpp"
	.intel_syntax noprefix
	.text
	.section	.text$_ZStanSt13_Ios_FmtflagsS_,"x"
	.linkonce discard
	.globl	_ZStanSt13_Ios_FmtflagsS_
	.def	_ZStanSt13_Ios_FmtflagsS_;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZStanSt13_Ios_FmtflagsS_
_ZStanSt13_Ios_FmtflagsS_:
.LFB1773:
	push	rbp
	.seh_pushreg	rbp
	mov	rbp, rsp
	.seh_setframe	rbp, 0
	.seh_endprologue
	mov	DWORD PTR 16[rbp], ecx
	mov	DWORD PTR 24[rbp], edx
	mov	eax, DWORD PTR 16[rbp]
	and	eax, DWORD PTR 24[rbp]
	pop	rbp
	ret
	.seh_endproc
	.section	.text$_ZStorSt13_Ios_FmtflagsS_,"x"
	.linkonce discard
	.globl	_ZStorSt13_Ios_FmtflagsS_
	.def	_ZStorSt13_Ios_FmtflagsS_;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZStorSt13_Ios_FmtflagsS_
_ZStorSt13_Ios_FmtflagsS_:
.LFB1774:
	push	rbp
	.seh_pushreg	rbp
	mov	rbp, rsp
	.seh_setframe	rbp, 0
	.seh_endprologue
	mov	DWORD PTR 16[rbp], ecx
	mov	DWORD PTR 24[rbp], edx
	mov	eax, DWORD PTR 16[rbp]
	or	eax, DWORD PTR 24[rbp]
	pop	rbp
	ret
	.seh_endproc
	.section	.text$_ZStcoSt13_Ios_Fmtflags,"x"
	.linkonce discard
	.globl	_ZStcoSt13_Ios_Fmtflags
	.def	_ZStcoSt13_Ios_Fmtflags;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZStcoSt13_Ios_Fmtflags
_ZStcoSt13_Ios_Fmtflags:
.LFB1776:
	push	rbp
	.seh_pushreg	rbp
	mov	rbp, rsp
	.seh_setframe	rbp, 0
	.seh_endprologue
	mov	DWORD PTR 16[rbp], ecx
	mov	eax, DWORD PTR 16[rbp]
	not	eax
	pop	rbp
	ret
	.seh_endproc
	.section	.text$_ZStoRRSt13_Ios_FmtflagsS_,"x"
	.linkonce discard
	.globl	_ZStoRRSt13_Ios_FmtflagsS_
	.def	_ZStoRRSt13_Ios_FmtflagsS_;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZStoRRSt13_Ios_FmtflagsS_
_ZStoRRSt13_Ios_FmtflagsS_:
.LFB1777:
	push	rbp
	.seh_pushreg	rbp
	mov	rbp, rsp
	.seh_setframe	rbp, 0
	sub	rsp, 32
	.seh_stackalloc	32
	.seh_endprologue
	mov	QWORD PTR 16[rbp], rcx
	mov	DWORD PTR 24[rbp], edx
	mov	rax, QWORD PTR 16[rbp]
	mov	eax, DWORD PTR [rax]
	mov	edx, DWORD PTR 24[rbp]
	mov	ecx, eax
	call	_ZStorSt13_Ios_FmtflagsS_
	mov	rdx, QWORD PTR 16[rbp]
	mov	DWORD PTR [rdx], eax
	mov	rax, QWORD PTR 16[rbp]
	add	rsp, 32
	pop	rbp
	ret
	.seh_endproc
	.section	.text$_ZStaNRSt13_Ios_FmtflagsS_,"x"
	.linkonce discard
	.globl	_ZStaNRSt13_Ios_FmtflagsS_
	.def	_ZStaNRSt13_Ios_FmtflagsS_;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZStaNRSt13_Ios_FmtflagsS_
_ZStaNRSt13_Ios_FmtflagsS_:
.LFB1778:
	push	rbp
	.seh_pushreg	rbp
	mov	rbp, rsp
	.seh_setframe	rbp, 0
	sub	rsp, 32
	.seh_stackalloc	32
	.seh_endprologue
	mov	QWORD PTR 16[rbp], rcx
	mov	DWORD PTR 24[rbp], edx
	mov	rax, QWORD PTR 16[rbp]
	mov	eax, DWORD PTR [rax]
	mov	edx, DWORD PTR 24[rbp]
	mov	ecx, eax
	call	_ZStanSt13_Ios_FmtflagsS_
	mov	rdx, QWORD PTR 16[rbp]
	mov	DWORD PTR [rdx], eax
	mov	rax, QWORD PTR 16[rbp]
	add	rsp, 32
	pop	rbp
	ret
	.seh_endproc
	.section	.text$_ZNSt8ios_base4setfESt13_Ios_FmtflagsS0_,"x"
	.linkonce discard
	.align 2
	.globl	_ZNSt8ios_base4setfESt13_Ios_FmtflagsS0_
	.def	_ZNSt8ios_base4setfESt13_Ios_FmtflagsS0_;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZNSt8ios_base4setfESt13_Ios_FmtflagsS0_
_ZNSt8ios_base4setfESt13_Ios_FmtflagsS0_:
.LFB1807:
	push	rbp
	.seh_pushreg	rbp
	mov	rbp, rsp
	.seh_setframe	rbp, 0
	sub	rsp, 48
	.seh_stackalloc	48
	.seh_endprologue
	mov	QWORD PTR 16[rbp], rcx
	mov	DWORD PTR 24[rbp], edx
	mov	DWORD PTR 32[rbp], r8d
	mov	rax, QWORD PTR 16[rbp]
	mov	eax, DWORD PTR 24[rax]
	mov	DWORD PTR -4[rbp], eax
	mov	eax, DWORD PTR 32[rbp]
	mov	ecx, eax
	call	_ZStcoSt13_Ios_Fmtflags
	mov	edx, eax
	mov	rax, QWORD PTR 16[rbp]
	add	rax, 24
	mov	rcx, rax
	call	_ZStaNRSt13_Ios_FmtflagsS_
	mov	edx, DWORD PTR 32[rbp]
	mov	eax, DWORD PTR 24[rbp]
	mov	ecx, eax
	call	_ZStanSt13_Ios_FmtflagsS_
	mov	edx, eax
	mov	rax, QWORD PTR 16[rbp]
	add	rax, 24
	mov	rcx, rax
	call	_ZStoRRSt13_Ios_FmtflagsS_
	mov	eax, DWORD PTR -4[rbp]
	add	rsp, 48
	pop	rbp
	ret
	.seh_endproc
	.section	.text$_ZSt3decRSt8ios_base,"x"
	.linkonce discard
	.globl	_ZSt3decRSt8ios_base
	.def	_ZSt3decRSt8ios_base;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZSt3decRSt8ios_base
_ZSt3decRSt8ios_base:
.LFB1834:
	push	rbp
	.seh_pushreg	rbp
	mov	rbp, rsp
	.seh_setframe	rbp, 0
	sub	rsp, 32
	.seh_stackalloc	32
	.seh_endprologue
	mov	QWORD PTR 16[rbp], rcx
	mov	rax, QWORD PTR 16[rbp]
	mov	r8d, 74
	mov	edx, 2
	mov	rcx, rax
	call	_ZNSt8ios_base4setfESt13_Ios_FmtflagsS0_
	mov	rax, QWORD PTR 16[rbp]
	add	rsp, 32
	pop	rbp
	ret
	.seh_endproc
	.section	.text$_ZSt3hexRSt8ios_base,"x"
	.linkonce discard
	.globl	_ZSt3hexRSt8ios_base
	.def	_ZSt3hexRSt8ios_base;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZSt3hexRSt8ios_base
_ZSt3hexRSt8ios_base:
.LFB1835:
	push	rbp
	.seh_pushreg	rbp
	mov	rbp, rsp
	.seh_setframe	rbp, 0
	sub	rsp, 32
	.seh_stackalloc	32
	.seh_endprologue
	mov	QWORD PTR 16[rbp], rcx
	mov	rax, QWORD PTR 16[rbp]
	mov	r8d, 74
	mov	edx, 8
	mov	rcx, rax
	call	_ZNSt8ios_base4setfESt13_Ios_FmtflagsS0_
	mov	rax, QWORD PTR 16[rbp]
	add	rsp, 32
	pop	rbp
	ret
	.seh_endproc
	.section .rdata,"dr"
	.align 8
.LC0:
	.ascii "[Test 1] Loopback Handshake Initializing...\0"
.LC1:
	.ascii "127.0.0.1\0"
	.align 8
.LC2:
	.ascii "Client connection failed! Error: \0"
.LC3:
	.ascii "Accept failed!\0"
	.align 8
.LC4:
	.ascii "[SUCCESS] Loopback Handshake Verified (Magic: \0"
.LC5:
	.ascii ")\0"
.LC6:
	.ascii "[FAILURE] Handshake Mismatch!\0"
	.align 8
.LC7:
	.ascii "[Test 2] Shard Transfer (MASM Kernel)...\0"
.LC8:
	.ascii "Pushing Shard Header...\0"
.LC9:
	.ascii "Push Result: \0"
.LC10:
	.ascii "Local sizeof(SwarmHeader): \0"
	.align 8
.LC11:
	.ascii "Local sizeof(SwarmTensorShard): \0"
.LC12:
	.ascii "Local offsetof(Size): \0"
.LC13:
	.ascii "Waiting for Header (Expected \0"
.LC14:
	.ascii " bytes)...\0"
	.align 8
.LC15:
	.ascii "Header recv failed or closed! r=\0"
.LC16:
	.ascii " err=\0"
.LC17:
	.ascii "  Received \0"
.LC18:
	.ascii " header bytes...\0"
.LC19:
	.ascii "Header Recv Bytes: \0"
.LC20:
	.ascii " Magic: \0"
.LC21:
	.ascii " Size: \0"
.LC22:
	.ascii "Pulling Shard Data...\0"
.LC23:
	.ascii "Pull Result: \0"
	.align 8
.LC24:
	.ascii "[SUCCESS] 1KB Shard Integrity Verified\0"
	.align 8
.LC25:
	.ascii "[FAILURE] Data Corruption in MASM Kernel Transfer!\0"
	.align 8
.LC26:
	.ascii "Sent[%d]: %02X Recv[%d]: %02X\12\0"
	.text
	.globl	_Z17run_loopback_testv
	.def	_Z17run_loopback_testv;	.scl	2;	.type	32;	.endef
	.seh_proc	_Z17run_loopback_testv
_Z17run_loopback_testv:
.LFB9010:
	push	rbp
	.seh_pushreg	rbp
	push	rdi
	.seh_pushreg	rdi
	push	rbx
	.seh_pushreg	rbx
	sub	rsp, 880
	.seh_stackalloc	880
	lea	rbp, 128[rsp]
	.seh_setframe	rbp, 128
	.seh_endprologue
	lea	rdx, .LC0[rip]
	mov	rax, QWORD PTR .refptr._ZSt4cout[rip]
	mov	rcx, rax
.LEHB0:
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
	mov	rcx, rax
	mov	rax, QWORD PTR .refptr._ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_[rip]
	mov	rdx, rax
	call	_ZNSolsEPFRSoS_E
	lea	rax, 240[rbp]
	mov	rdx, rax
	mov	ecx, 514
	mov	rax, QWORD PTR __imp_WSAStartup[rip]
	call	rax
	mov	r8d, 6
	mov	edx, 1
	mov	ecx, 2
	mov	rax, QWORD PTR __imp_socket[rip]
	call	rax
	mov	QWORD PTR 736[rbp], rax
	mov	QWORD PTR 224[rbp], 0
	mov	QWORD PTR 232[rbp], 0
	mov	WORD PTR 224[rbp], 2
	mov	ecx, 1337
	mov	rax, QWORD PTR __imp_htons[rip]
	call	rax
	mov	WORD PTR 226[rbp], ax
	mov	DWORD PTR 228[rbp], 0
	lea	rax, 224[rbp]
	mov	rcx, QWORD PTR 736[rbp]
	mov	r8d, 16
	mov	rdx, rax
	mov	rax, QWORD PTR __imp_bind[rip]
	call	rax
	mov	rax, QWORD PTR 736[rbp]
	mov	edx, 1
	mov	rcx, rax
	mov	rax, QWORD PTR __imp_listen[rip]
	call	rax
	mov	WORD PTR 734[rbp], 1337
	mov	QWORD PTR 208[rbp], 0
	mov	QWORD PTR 216[rbp], 0
	mov	WORD PTR 208[rbp], 2
	movzx	eax, WORD PTR 734[rbp]
	mov	ecx, eax
	mov	rax, QWORD PTR __imp_htons[rip]
	call	rax
	mov	WORD PTR 210[rbp], ax
	lea	rax, 208[rbp]
	lea	rdx, 4[rax]
	lea	rax, .LC1[rip]
	mov	r8, rdx
	mov	rdx, rax
	mov	ecx, 2
	mov	rax, QWORD PTR __imp_inet_pton[rip]
	call	rax
	mov	ecx, 100
	mov	rax, QWORD PTR __imp_Sleep[rip]
	call	rax
	mov	r8d, 6
	mov	edx, 1
	mov	ecx, 2
	mov	rax, QWORD PTR __imp_socket[rip]
	call	rax
	mov	QWORD PTR 720[rbp], rax
	lea	rax, 208[rbp]
	mov	rcx, QWORD PTR 720[rbp]
	mov	r8d, 16
	mov	rdx, rax
	mov	rax, QWORD PTR __imp_connect[rip]
	call	rax
	cmp	eax, -1
	sete	al
	test	al, al
	je	.L18
	mov	rax, QWORD PTR __imp_WSAGetLastError[rip]
	call	rax
	mov	DWORD PTR 676[rbp], eax
	lea	rdx, .LC2[rip]
	mov	rax, QWORD PTR .refptr._ZSt4cerr[rip]
	mov	rcx, rax
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
	mov	rcx, rax
	mov	eax, DWORD PTR 676[rbp]
	mov	edx, eax
	call	_ZNSolsEi
	mov	rcx, rax
	mov	rax, QWORD PTR .refptr._ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_[rip]
	mov	rdx, rax
	call	_ZNSolsEPFRSoS_E
	mov	rax, QWORD PTR 736[rbp]
	mov	rcx, rax
	mov	rax, QWORD PTR __imp_closesocket[rip]
	call	rax
	mov	rax, QWORD PTR __imp_WSACleanup[rip]
	call	rax
	mov	ebx, 0
	jmp	.L32
.L18:
	mov	rax, QWORD PTR 736[rbp]
	mov	r8d, 0
	mov	edx, 0
	mov	rcx, rax
	mov	rax, QWORD PTR __imp_accept[rip]
	call	rax
	mov	QWORD PTR 712[rbp], rax
	cmp	QWORD PTR 712[rbp], -1
	jne	.L20
	lea	rdx, .LC3[rip]
	mov	rax, QWORD PTR .refptr._ZSt4cerr[rip]
	mov	rcx, rax
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
	mov	rcx, rax
	mov	rax, QWORD PTR .refptr._ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_[rip]
	mov	rdx, rax
	call	_ZNSolsEPFRSoS_E
	mov	ebx, 0
	jmp	.L32
.L20:
	lea	rdx, 160[rbp]
	mov	eax, 0
	mov	ecx, 6
	mov	rdi, rdx
	rep stosq
	mov	DWORD PTR 160[rbp], 1297241939
	mov	DWORD PTR 164[rbp], 16908288
	mov	QWORD PTR 184[rbp], 101
	mov	QWORD PTR 192[rbp], 2
	lea	rax, 160[rbp]
	mov	rcx, QWORD PTR 720[rbp]
	mov	r9d, 0
	mov	r8d, 48
	mov	rdx, rax
	call	RawrXD_Swarm_SendBuffer
	lea	rdx, 112[rbp]
	mov	eax, 0
	mov	ecx, 6
	mov	rdi, rdx
	rep stosq
	lea	rax, 112[rbp]
	mov	rcx, QWORD PTR 712[rbp]
	mov	r9d, 0
	mov	r8d, 48
	mov	rdx, rax
	call	RawrXD_Swarm_RecvBuffer
	mov	edx, DWORD PTR 112[rbp]
	mov	eax, DWORD PTR 160[rbp]
	cmp	edx, eax
	jne	.L21
	mov	rax, QWORD PTR 136[rbp]
	cmp	rax, 101
	jne	.L21
	lea	rdx, .LC4[rip]
	mov	rax, QWORD PTR .refptr._ZSt4cout[rip]
	mov	rcx, rax
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
	lea	rdx, _ZSt3hexRSt8ios_base[rip]
	mov	rcx, rax
	call	_ZNSolsEPFRSt8ios_baseS0_E
	mov	rcx, rax
	mov	eax, DWORD PTR 112[rbp]
	mov	edx, eax
	call	_ZNSolsEj
	mov	rcx, rax
	lea	rax, .LC5[rip]
	mov	rdx, rax
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
	mov	rcx, rax
	mov	rax, QWORD PTR .refptr._ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_[rip]
	mov	rdx, rax
	call	_ZNSolsEPFRSoS_E
.LEHE0:
	nop
	lea	rax, 652[rbp]
	mov	QWORD PTR 664[rbp], rax
	nop
	nop
	mov	BYTE PTR 653[rbp], -86
	lea	rcx, 652[rbp]
	lea	rdx, 653[rbp]
	lea	rax, 80[rbp]
	mov	r9, rcx
	mov	r8, rdx
	mov	edx, 1024
	mov	rcx, rax
.LEHB1:
	call	_ZNSt6vectorIhSaIhEEC1EyRKhRKS0_
.LEHE1:
	jmp	.L41
.L21:
	lea	rdx, .LC6[rip]
	mov	rax, QWORD PTR .refptr._ZSt4cerr[rip]
	mov	rcx, rax
.LEHB2:
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
	mov	rcx, rax
	mov	rax, QWORD PTR .refptr._ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_[rip]
	mov	rdx, rax
	call	_ZNSolsEPFRSoS_E
.LEHE2:
	mov	ebx, 0
	jmp	.L32
.L41:
	lea	rax, 652[rbp]
	mov	rcx, rax
	call	_ZNSt15__new_allocatorIhED2Ev
	nop
	lea	rdx, 16[rbp]
	mov	eax, 0
	mov	ecx, 7
	mov	rdi, rdx
	rep stosq
	mov	DWORD PTR 16[rbp], 1297241939
	lea	rax, 80[rbp]
	mov	rcx, rax
	call	_ZNKSt6vectorIhSaIhEE4sizeEv
	mov	QWORD PTR 56[rbp], rax
	mov	QWORD PTR 40[rbp], 999
	lea	rdx, .LC7[rip]
	mov	rax, QWORD PTR .refptr._ZSt4cout[rip]
	mov	rcx, rax
.LEHB3:
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
	mov	rcx, rax
	mov	rax, QWORD PTR .refptr._ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_[rip]
	mov	rdx, rax
	call	_ZNSolsEPFRSoS_E
	lea	rdx, .LC8[rip]
	mov	rax, QWORD PTR .refptr._ZSt4cout[rip]
	mov	rcx, rax
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
	mov	rcx, rax
	mov	rax, QWORD PTR .refptr._ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_[rip]
	mov	rdx, rax
	call	_ZNSolsEPFRSoS_E
	lea	rax, 80[rbp]
	mov	rcx, rax
	call	_ZNSt6vectorIhSaIhEE4dataEv
	mov	rdx, rax
	lea	rax, 16[rbp]
	mov	rcx, QWORD PTR 720[rbp]
	mov	r9d, 0
	mov	r8, rdx
	mov	rdx, rax
	call	RawrXD_Swarm_SyncTensorShard
	mov	QWORD PTR 704[rbp], rax
	lea	rdx, .LC9[rip]
	mov	rax, QWORD PTR .refptr._ZSt4cout[rip]
	mov	rcx, rax
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
	mov	rcx, rax
	mov	rax, QWORD PTR 704[rbp]
	mov	rdx, rax
	call	_ZNSolsEx
	mov	rcx, rax
	mov	rax, QWORD PTR .refptr._ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_[rip]
	mov	rdx, rax
	call	_ZNSolsEPFRSoS_E
.LEHE3:
	lea	rax, 654[rbp]
	mov	QWORD PTR 656[rbp], rax
	nop
	nop
	mov	BYTE PTR 655[rbp], 0
	lea	rcx, 654[rbp]
	lea	rdx, 655[rbp]
	lea	rax, -16[rbp]
	mov	r9, rcx
	mov	r8, rdx
	mov	edx, 1024
	mov	rcx, rax
.LEHB4:
	call	_ZNSt6vectorIhSaIhEEC1EyRKhRKS0_
.LEHE4:
	lea	rax, 654[rbp]
	mov	rcx, rax
	call	_ZNSt15__new_allocatorIhED2Ev
	nop
	lea	rdx, -80[rbp]
	mov	eax, 0
	mov	ecx, 7
	mov	rdi, rdx
	rep stosq
	lea	rdx, .LC10[rip]
	mov	rax, QWORD PTR .refptr._ZSt4cout[rip]
	mov	rcx, rax
.LEHB5:
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
	mov	edx, 24
	mov	rcx, rax
	call	_ZNSolsEy
	mov	rcx, rax
	mov	rax, QWORD PTR .refptr._ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_[rip]
	mov	rdx, rax
	call	_ZNSolsEPFRSoS_E
	lea	rdx, .LC11[rip]
	mov	rax, QWORD PTR .refptr._ZSt4cout[rip]
	mov	rcx, rax
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
	mov	edx, 56
	mov	rcx, rax
	call	_ZNSolsEy
	mov	rcx, rax
	mov	rax, QWORD PTR .refptr._ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_[rip]
	mov	rdx, rax
	call	_ZNSolsEPFRSoS_E
	lea	rdx, .LC12[rip]
	mov	rax, QWORD PTR .refptr._ZSt4cout[rip]
	mov	rcx, rax
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
	mov	edx, 40
	mov	rcx, rax
	call	_ZNSolsEy
	mov	rcx, rax
	mov	rax, QWORD PTR .refptr._ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_[rip]
	mov	rdx, rax
	call	_ZNSolsEPFRSoS_E
	lea	rdx, .LC13[rip]
	mov	rax, QWORD PTR .refptr._ZSt4cout[rip]
	mov	rcx, rax
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
	mov	edx, 56
	mov	rcx, rax
	call	_ZNSolsEy
	mov	rcx, rax
	lea	rax, .LC14[rip]
	mov	rdx, rax
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
	mov	rcx, rax
	mov	rax, QWORD PTR .refptr._ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_[rip]
	mov	rdx, rax
	call	_ZNSolsEPFRSoS_E
	lea	rax, -80[rbp]
	mov	QWORD PTR 696[rbp], rax
	mov	DWORD PTR 692[rbp], 56
	mov	DWORD PTR 748[rbp], 0
	jmp	.L23
.L26:
	mov	eax, DWORD PTR 692[rbp]
	sub	eax, DWORD PTR 748[rbp]
	mov	ecx, eax
	mov	eax, DWORD PTR 748[rbp]
	cdqe
	mov	rdx, QWORD PTR 696[rbp]
	add	rdx, rax
	mov	rax, QWORD PTR 712[rbp]
	mov	r9d, 0
	mov	r8d, ecx
	mov	rcx, rax
	mov	rax, QWORD PTR __imp_recv[rip]
	call	rax
	mov	DWORD PTR 688[rbp], eax
	cmp	DWORD PTR 688[rbp], 0
	jg	.L24
	lea	rdx, .LC15[rip]
	mov	rax, QWORD PTR .refptr._ZSt4cerr[rip]
	mov	rcx, rax
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
	mov	rcx, rax
	mov	eax, DWORD PTR 688[rbp]
	mov	edx, eax
	call	_ZNSolsEi
	mov	rcx, rax
	lea	rax, .LC16[rip]
	mov	rdx, rax
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
	mov	rbx, rax
	mov	rax, QWORD PTR __imp_WSAGetLastError[rip]
	call	rax
	mov	edx, eax
	mov	rcx, rbx
	call	_ZNSolsEi
	mov	rcx, rax
	mov	rax, QWORD PTR .refptr._ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_[rip]
	mov	rdx, rax
	call	_ZNSolsEPFRSoS_E
	jmp	.L25
.L24:
	mov	eax, DWORD PTR 688[rbp]
	add	DWORD PTR 748[rbp], eax
	lea	rdx, .LC17[rip]
	mov	rax, QWORD PTR .refptr._ZSt4cout[rip]
	mov	rcx, rax
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
	mov	rcx, rax
	mov	eax, DWORD PTR 688[rbp]
	mov	edx, eax
	call	_ZNSolsEi
	mov	rcx, rax
	lea	rax, .LC18[rip]
	mov	rdx, rax
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
	mov	rcx, rax
	mov	rax, QWORD PTR .refptr._ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_[rip]
	mov	rdx, rax
	call	_ZNSolsEPFRSoS_E
.L23:
	mov	eax, DWORD PTR 748[rbp]
	cmp	eax, DWORD PTR 692[rbp]
	jl	.L26
.L25:
	lea	rdx, .LC19[rip]
	mov	rax, QWORD PTR .refptr._ZSt4cout[rip]
	mov	rcx, rax
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
	mov	rcx, rax
	mov	eax, DWORD PTR 748[rbp]
	mov	edx, eax
	call	_ZNSolsEi
	mov	rcx, rax
	lea	rax, .LC20[rip]
	mov	rdx, rax
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
	lea	rdx, _ZSt3hexRSt8ios_base[rip]
	mov	rcx, rax
	call	_ZNSolsEPFRSt8ios_baseS0_E
	mov	rcx, rax
	mov	eax, DWORD PTR -80[rbp]
	mov	edx, eax
	call	_ZNSolsEj
	lea	rdx, _ZSt3decRSt8ios_base[rip]
	mov	rcx, rax
	call	_ZNSolsEPFRSt8ios_baseS0_E
	mov	rcx, rax
	lea	rax, .LC21[rip]
	mov	rdx, rax
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
	mov	rcx, rax
	mov	rax, QWORD PTR -40[rbp]
	mov	rdx, rax
	call	_ZNSolsEy
	mov	rcx, rax
	mov	rax, QWORD PTR .refptr._ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_[rip]
	mov	rdx, rax
	call	_ZNSolsEPFRSoS_E
	lea	rdx, .LC22[rip]
	mov	rax, QWORD PTR .refptr._ZSt4cout[rip]
	mov	rcx, rax
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
	mov	rcx, rax
	mov	rax, QWORD PTR .refptr._ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_[rip]
	mov	rdx, rax
	call	_ZNSolsEPFRSoS_E
	lea	rax, -16[rbp]
	mov	rcx, rax
	call	_ZNSt6vectorIhSaIhEE4dataEv
	mov	rdx, rax
	lea	rax, -80[rbp]
	mov	rcx, QWORD PTR 712[rbp]
	mov	r9d, 1
	mov	r8, rdx
	mov	rdx, rax
	call	RawrXD_Swarm_SyncTensorShard
	mov	QWORD PTR 680[rbp], rax
	lea	rdx, .LC23[rip]
	mov	rax, QWORD PTR .refptr._ZSt4cout[rip]
	mov	rcx, rax
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
	mov	rcx, rax
	mov	rax, QWORD PTR 680[rbp]
	mov	rdx, rax
	call	_ZNSolsEx
	mov	rcx, rax
	mov	rax, QWORD PTR .refptr._ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_[rip]
	mov	rdx, rax
	call	_ZNSolsEPFRSoS_E
	lea	rdx, 80[rbp]
	lea	rax, -16[rbp]
	mov	rcx, rax
	call	_ZSteqIhSaIhEEbRKSt6vectorIT_T0_ES6_
	test	al, al
	je	.L27
	lea	rdx, .LC24[rip]
	mov	rax, QWORD PTR .refptr._ZSt4cout[rip]
	mov	rcx, rax
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
	mov	rcx, rax
	mov	rax, QWORD PTR .refptr._ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_[rip]
	mov	rdx, rax
	call	_ZNSolsEPFRSoS_E
	jmp	.L42
.L27:
	lea	rdx, .LC25[rip]
	mov	rax, QWORD PTR .refptr._ZSt4cerr[rip]
	mov	rcx, rax
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
	mov	rcx, rax
	mov	rax, QWORD PTR .refptr._ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_[rip]
	mov	rdx, rax
	call	_ZNSolsEPFRSoS_E
	mov	DWORD PTR 744[rbp], 0
	jmp	.L29
.L30:
	mov	eax, DWORD PTR 744[rbp]
	movsx	rdx, eax
	lea	rax, -16[rbp]
	mov	rcx, rax
	call	_ZNSt6vectorIhSaIhEEixEy
	movzx	eax, BYTE PTR [rax]
	movzx	ebx, al
	mov	eax, DWORD PTR 744[rbp]
	movsx	rdx, eax
	lea	rax, 80[rbp]
	mov	rcx, rax
	call	_ZNSt6vectorIhSaIhEEixEy
	movzx	eax, BYTE PTR [rax]
	movzx	ecx, al
	mov	r8d, DWORD PTR 744[rbp]
	mov	edx, DWORD PTR 744[rbp]
	lea	rax, .LC26[rip]
	mov	DWORD PTR 32[rsp], ebx
	mov	r9d, r8d
	mov	r8d, ecx
	mov	rcx, rax
	call	__mingw_printf
	add	DWORD PTR 744[rbp], 1
.L29:
	cmp	DWORD PTR 744[rbp], 15
	jle	.L30
	mov	ebx, 0
	jmp	.L31
.L42:
	mov	rax, QWORD PTR 720[rbp]
	mov	rcx, rax
	mov	rax, QWORD PTR __imp_closesocket[rip]
	call	rax
	mov	rax, QWORD PTR 712[rbp]
	mov	rcx, rax
	mov	rax, QWORD PTR __imp_closesocket[rip]
	call	rax
	mov	rax, QWORD PTR 736[rbp]
	mov	rcx, rax
	mov	rax, QWORD PTR __imp_closesocket[rip]
	call	rax
	mov	rax, QWORD PTR __imp_WSACleanup[rip]
	call	rax
.LEHE5:
	mov	ebx, 1
.L31:
	lea	rax, -16[rbp]
	mov	rcx, rax
	call	_ZNSt6vectorIhSaIhEED1Ev
	lea	rax, 80[rbp]
	mov	rcx, rax
	call	_ZNSt6vectorIhSaIhEED1Ev
.L32:
	mov	eax, ebx
	jmp	.L43
.L37:
	mov	rbx, rax
	lea	rax, 652[rbp]
	mov	rcx, rax
	call	_ZNSt15__new_allocatorIhED2Ev
	nop
	mov	rax, rbx
	mov	rcx, rax
.LEHB6:
	call	_Unwind_Resume
.L39:
	mov	rbx, rax
	lea	rax, 654[rbp]
	mov	rcx, rax
	call	_ZNSt15__new_allocatorIhED2Ev
	nop
	jmp	.L35
.L40:
	mov	rbx, rax
	lea	rax, -16[rbp]
	mov	rcx, rax
	call	_ZNSt6vectorIhSaIhEED1Ev
	jmp	.L35
.L38:
	mov	rbx, rax
.L35:
	lea	rax, 80[rbp]
	mov	rcx, rax
	call	_ZNSt6vectorIhSaIhEED1Ev
	mov	rax, rbx
	mov	rcx, rax
	call	_Unwind_Resume
.LEHE6:
.L43:
	add	rsp, 880
	pop	rbx
	pop	rdi
	pop	rbp
	ret
	.seh_handler	__gxx_personality_seh0, @unwind, @except
	.seh_handlerdata
.LLSDA9010:
	.byte	0xff
	.byte	0xff
	.byte	0x1
	.uleb128 .LLSDACSE9010-.LLSDACSB9010
.LLSDACSB9010:
	.uleb128 .LEHB0-.LFB9010
	.uleb128 .LEHE0-.LEHB0
	.uleb128 0
	.uleb128 0
	.uleb128 .LEHB1-.LFB9010
	.uleb128 .LEHE1-.LEHB1
	.uleb128 .L37-.LFB9010
	.uleb128 0
	.uleb128 .LEHB2-.LFB9010
	.uleb128 .LEHE2-.LEHB2
	.uleb128 0
	.uleb128 0
	.uleb128 .LEHB3-.LFB9010
	.uleb128 .LEHE3-.LEHB3
	.uleb128 .L38-.LFB9010
	.uleb128 0
	.uleb128 .LEHB4-.LFB9010
	.uleb128 .LEHE4-.LEHB4
	.uleb128 .L39-.LFB9010
	.uleb128 0
	.uleb128 .LEHB5-.LFB9010
	.uleb128 .LEHE5-.LEHB5
	.uleb128 .L40-.LFB9010
	.uleb128 0
	.uleb128 .LEHB6-.LFB9010
	.uleb128 .LEHE6-.LEHB6
	.uleb128 0
	.uleb128 0
.LLSDACSE9010:
	.text
	.seh_endproc
	.globl	main
	.def	main;	.scl	2;	.type	32;	.endef
	.seh_proc	main
main:
.LFB9011:
	push	rbp
	.seh_pushreg	rbp
	mov	rbp, rsp
	.seh_setframe	rbp, 0
	sub	rsp, 32
	.seh_stackalloc	32
	.seh_endprologue
	mov	DWORD PTR 16[rbp], ecx
	mov	QWORD PTR 24[rbp], rdx
	call	__main
	call	_Z17run_loopback_testv
	test	al, al
	je	.L45
	mov	eax, 0
	jmp	.L46
.L45:
	mov	eax, 1
.L46:
	add	rsp, 32
	pop	rbp
	ret
	.seh_endproc
	.section	.text$_ZNSt6vectorIhSaIhEEC1EyRKhRKS0_,"x"
	.linkonce discard
	.align 2
	.globl	_ZNSt6vectorIhSaIhEEC1EyRKhRKS0_
	.def	_ZNSt6vectorIhSaIhEEC1EyRKhRKS0_;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZNSt6vectorIhSaIhEEC1EyRKhRKS0_
_ZNSt6vectorIhSaIhEEC1EyRKhRKS0_:
.LFB9301:
	push	rbp
	.seh_pushreg	rbp
	push	rbx
	.seh_pushreg	rbx
	sub	rsp, 40
	.seh_stackalloc	40
	lea	rbp, 32[rsp]
	.seh_setframe	rbp, 32
	.seh_endprologue
	mov	QWORD PTR 32[rbp], rcx
	mov	QWORD PTR 40[rbp], rdx
	mov	QWORD PTR 48[rbp], r8
	mov	QWORD PTR 56[rbp], r9
	mov	rbx, QWORD PTR 32[rbp]
	mov	rdx, QWORD PTR 56[rbp]
	mov	rax, QWORD PTR 40[rbp]
	mov	rcx, rax
.LEHB7:
	call	_ZNSt6vectorIhSaIhEE17_S_check_init_lenEyRKS0_
	mov	rdx, QWORD PTR 56[rbp]
	mov	r8, rdx
	mov	rdx, rax
	mov	rcx, rbx
	call	_ZNSt12_Vector_baseIhSaIhEEC2EyRKS0_
.LEHE7:
	mov	rcx, QWORD PTR 48[rbp]
	mov	rdx, QWORD PTR 40[rbp]
	mov	rax, QWORD PTR 32[rbp]
	mov	r8, rcx
	mov	rcx, rax
.LEHB8:
	call	_ZNSt6vectorIhSaIhEE18_M_fill_initializeEyRKh
.LEHE8:
	jmp	.L50
.L49:
	mov	rbx, rax
	mov	rax, QWORD PTR 32[rbp]
	mov	rcx, rax
	call	_ZNSt12_Vector_baseIhSaIhEED2Ev
	mov	rax, rbx
	mov	rcx, rax
.LEHB9:
	call	_Unwind_Resume
	nop
.LEHE9:
.L50:
	add	rsp, 40
	pop	rbx
	pop	rbp
	ret
	.seh_handler	__gxx_personality_seh0, @unwind, @except
	.seh_handlerdata
.LLSDA9301:
	.byte	0xff
	.byte	0xff
	.byte	0x1
	.uleb128 .LLSDACSE9301-.LLSDACSB9301
.LLSDACSB9301:
	.uleb128 .LEHB7-.LFB9301
	.uleb128 .LEHE7-.LEHB7
	.uleb128 0
	.uleb128 0
	.uleb128 .LEHB8-.LFB9301
	.uleb128 .LEHE8-.LEHB8
	.uleb128 .L49-.LFB9301
	.uleb128 0
	.uleb128 .LEHB9-.LFB9301
	.uleb128 .LEHE9-.LEHB9
	.uleb128 0
	.uleb128 0
.LLSDACSE9301:
	.section	.text$_ZNSt6vectorIhSaIhEEC1EyRKhRKS0_,"x"
	.linkonce discard
	.seh_endproc
	.section	.text$_ZNSt6vectorIhSaIhEED1Ev,"x"
	.linkonce discard
	.align 2
	.globl	_ZNSt6vectorIhSaIhEED1Ev
	.def	_ZNSt6vectorIhSaIhEED1Ev;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZNSt6vectorIhSaIhEED1Ev
_ZNSt6vectorIhSaIhEED1Ev:
.LFB9304:
	push	rbp
	.seh_pushreg	rbp
	mov	rbp, rsp
	.seh_setframe	rbp, 0
	sub	rsp, 64
	.seh_stackalloc	64
	.seh_endprologue
	mov	QWORD PTR 16[rbp], rcx
	mov	rax, QWORD PTR 16[rbp]
	mov	rcx, rax
	call	_ZNSt12_Vector_baseIhSaIhEE19_M_get_Tp_allocatorEv
	mov	rdx, QWORD PTR 16[rbp]
	mov	rdx, QWORD PTR 8[rdx]
	mov	rcx, QWORD PTR 16[rbp]
	mov	rcx, QWORD PTR [rcx]
	mov	QWORD PTR -8[rbp], rcx
	mov	QWORD PTR -16[rbp], rdx
	mov	QWORD PTR -24[rbp], rax
	mov	rdx, QWORD PTR -16[rbp]
	mov	rax, QWORD PTR -8[rbp]
	mov	rcx, rax
	call	_ZSt8_DestroyIPhEvT_S1_
	nop
	mov	rax, QWORD PTR 16[rbp]
	mov	rcx, rax
	call	_ZNSt12_Vector_baseIhSaIhEED2Ev
	nop
	add	rsp, 64
	pop	rbp
	ret
	.seh_endproc
	.section	.text$_ZNKSt6vectorIhSaIhEE4sizeEv,"x"
	.linkonce discard
	.align 2
	.globl	_ZNKSt6vectorIhSaIhEE4sizeEv
	.def	_ZNKSt6vectorIhSaIhEE4sizeEv;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZNKSt6vectorIhSaIhEE4sizeEv
_ZNKSt6vectorIhSaIhEE4sizeEv:
.LFB9305:
	push	rbp
	.seh_pushreg	rbp
	mov	rbp, rsp
	.seh_setframe	rbp, 0
	sub	rsp, 16
	.seh_stackalloc	16
	.seh_endprologue
	mov	QWORD PTR 16[rbp], rcx
	mov	rax, QWORD PTR 16[rbp]
	mov	rcx, QWORD PTR 8[rax]
	mov	rax, QWORD PTR 16[rbp]
	mov	rdx, QWORD PTR [rax]
	mov	rax, rcx
	sub	rax, rdx
	mov	QWORD PTR -8[rbp], rax
	cmp	QWORD PTR -8[rbp], 0
	mov	rax, QWORD PTR -8[rbp]
	add	rsp, 16
	pop	rbp
	ret
	.seh_endproc
	.section	.text$_ZNSt6vectorIhSaIhEE4dataEv,"x"
	.linkonce discard
	.align 2
	.globl	_ZNSt6vectorIhSaIhEE4dataEv
	.def	_ZNSt6vectorIhSaIhEE4dataEv;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZNSt6vectorIhSaIhEE4dataEv
_ZNSt6vectorIhSaIhEE4dataEv:
.LFB9306:
	push	rbp
	.seh_pushreg	rbp
	mov	rbp, rsp
	.seh_setframe	rbp, 0
	sub	rsp, 32
	.seh_stackalloc	32
	.seh_endprologue
	mov	QWORD PTR 16[rbp], rcx
	mov	rax, QWORD PTR 16[rbp]
	mov	rdx, QWORD PTR [rax]
	mov	rax, QWORD PTR 16[rbp]
	mov	rcx, rax
	call	_ZNKSt6vectorIhSaIhEE11_M_data_ptrIhEEPT_S4_
	add	rsp, 32
	pop	rbp
	ret
	.seh_endproc
	.section	.text$_ZSteqIhSaIhEEbRKSt6vectorIT_T0_ES6_,"x"
	.linkonce discard
	.globl	_ZSteqIhSaIhEEbRKSt6vectorIT_T0_ES6_
	.def	_ZSteqIhSaIhEEbRKSt6vectorIT_T0_ES6_;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZSteqIhSaIhEEbRKSt6vectorIT_T0_ES6_
_ZSteqIhSaIhEEbRKSt6vectorIT_T0_ES6_:
.LFB9309:
	push	rbp
	.seh_pushreg	rbp
	push	rsi
	.seh_pushreg	rsi
	push	rbx
	.seh_pushreg	rbx
	sub	rsp, 32
	.seh_stackalloc	32
	lea	rbp, 32[rsp]
	.seh_setframe	rbp, 32
	.seh_endprologue
	mov	QWORD PTR 32[rbp], rcx
	mov	QWORD PTR 40[rbp], rdx
	mov	rax, QWORD PTR 32[rbp]
	mov	rcx, rax
	call	_ZNKSt6vectorIhSaIhEE4sizeEv
	mov	rbx, rax
	mov	rax, QWORD PTR 40[rbp]
	mov	rcx, rax
	call	_ZNKSt6vectorIhSaIhEE4sizeEv
	cmp	rbx, rax
	jne	.L58
	mov	rax, QWORD PTR 40[rbp]
	mov	rcx, rax
	call	_ZNKSt6vectorIhSaIhEE5beginEv
	mov	rsi, rax
	mov	rax, QWORD PTR 32[rbp]
	mov	rcx, rax
	call	_ZNKSt6vectorIhSaIhEE3endEv
	mov	rbx, rax
	mov	rax, QWORD PTR 32[rbp]
	mov	rcx, rax
	call	_ZNKSt6vectorIhSaIhEE5beginEv
	mov	r8, rsi
	mov	rdx, rbx
	mov	rcx, rax
	call	_ZSt5equalIN9__gnu_cxx17__normal_iteratorIPKhSt6vectorIhSaIhEEEES7_EbT_S8_T0_
	test	al, al
	je	.L58
	mov	eax, 1
	jmp	.L59
.L58:
	mov	eax, 0
.L59:
	add	rsp, 32
	pop	rbx
	pop	rsi
	pop	rbp
	ret
	.seh_endproc
	.section .rdata,"dr"
.LC27:
	.ascii "__n < this->size()\0"
	.align 8
.LC28:
	.ascii "std::vector<_Tp, _Alloc>::reference std::vector<_Tp, _Alloc>::operator[](size_type) [with _Tp = unsigned char; _Alloc = std::allocator<unsigned char>; reference = unsigned char&; size_type = long long unsigned int]\0"
	.align 8
.LC29:
	.ascii "C:/ProgramData/mingw64/mingw64/lib/gcc/x86_64-w64-mingw32/15.2.0/include/c++/bits/stl_vector.h\0"
	.section	.text$_ZNSt6vectorIhSaIhEEixEy,"x"
	.linkonce discard
	.align 2
	.globl	_ZNSt6vectorIhSaIhEEixEy
	.def	_ZNSt6vectorIhSaIhEEixEy;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZNSt6vectorIhSaIhEEixEy
_ZNSt6vectorIhSaIhEEixEy:
.LFB9310:
	push	rbp
	.seh_pushreg	rbp
	mov	rbp, rsp
	.seh_setframe	rbp, 0
	sub	rsp, 32
	.seh_stackalloc	32
	.seh_endprologue
	mov	QWORD PTR 16[rbp], rcx
	mov	QWORD PTR 24[rbp], rdx
	mov	rax, QWORD PTR 16[rbp]
	mov	rcx, rax
	call	_ZNKSt6vectorIhSaIhEE4sizeEv
	cmp	QWORD PTR 24[rbp], rax
	setnb	al
	movzx	eax, al
	test	eax, eax
	setne	al
	test	al, al
	je	.L62
	lea	rcx, .LC27[rip]
	lea	rdx, .LC28[rip]
	lea	rax, .LC29[rip]
	mov	r9, rcx
	mov	r8, rdx
	mov	edx, 1263
	mov	rcx, rax
	call	_ZSt21__glibcxx_assert_failPKciS0_S0_
.L62:
	mov	rax, QWORD PTR 16[rbp]
	mov	rdx, QWORD PTR [rax]
	mov	rax, QWORD PTR 24[rbp]
	add	rax, rdx
	add	rsp, 32
	pop	rbp
	ret
	.seh_endproc
	.section	.text$_ZNSt15__new_allocatorIhED2Ev,"x"
	.linkonce discard
	.align 2
	.globl	_ZNSt15__new_allocatorIhED2Ev
	.def	_ZNSt15__new_allocatorIhED2Ev;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZNSt15__new_allocatorIhED2Ev
_ZNSt15__new_allocatorIhED2Ev:
.LFB9431:
	push	rbp
	.seh_pushreg	rbp
	mov	rbp, rsp
	.seh_setframe	rbp, 0
	.seh_endprologue
	mov	QWORD PTR 16[rbp], rcx
	nop
	pop	rbp
	ret
	.seh_endproc
	.section .rdata,"dr"
	.align 8
.LC30:
	.ascii "cannot create std::vector larger than max_size()\0"
	.section	.text$_ZNSt6vectorIhSaIhEE17_S_check_init_lenEyRKS0_,"x"
	.linkonce discard
	.globl	_ZNSt6vectorIhSaIhEE17_S_check_init_lenEyRKS0_
	.def	_ZNSt6vectorIhSaIhEE17_S_check_init_lenEyRKS0_;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZNSt6vectorIhSaIhEE17_S_check_init_lenEyRKS0_
_ZNSt6vectorIhSaIhEE17_S_check_init_lenEyRKS0_:
.LFB9433:
	push	rbp
	.seh_pushreg	rbp
	push	rbx
	.seh_pushreg	rbx
	sub	rsp, 72
	.seh_stackalloc	72
	lea	rbp, 64[rsp]
	.seh_setframe	rbp, 64
	.seh_endprologue
	mov	QWORD PTR 32[rbp], rcx
	mov	QWORD PTR 40[rbp], rdx
	mov	rax, QWORD PTR 40[rbp]
	mov	QWORD PTR -8[rbp], rax
	lea	rax, -25[rbp]
	mov	QWORD PTR -16[rbp], rax
	mov	rax, QWORD PTR -8[rbp]
	mov	QWORD PTR -24[rbp], rax
	nop
	nop
	lea	rax, -25[rbp]
	mov	rcx, rax
	call	_ZNSt6vectorIhSaIhEE11_S_max_sizeERKS0_
	cmp	rax, QWORD PTR 32[rbp]
	setb	bl
	lea	rax, -25[rbp]
	mov	rcx, rax
	call	_ZNSt15__new_allocatorIhED2Ev
	nop
	test	bl, bl
	je	.L66
	lea	rax, .LC30[rip]
	mov	rcx, rax
	call	_ZSt20__throw_length_errorPKc
.L66:
	mov	rax, QWORD PTR 32[rbp]
	add	rsp, 72
	pop	rbx
	pop	rbp
	ret
	.seh_endproc
	.section	.text$_ZNSt12_Vector_baseIhSaIhEE12_Vector_implD1Ev,"x"
	.linkonce discard
	.align 2
	.globl	_ZNSt12_Vector_baseIhSaIhEE12_Vector_implD1Ev
	.def	_ZNSt12_Vector_baseIhSaIhEE12_Vector_implD1Ev;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZNSt12_Vector_baseIhSaIhEE12_Vector_implD1Ev
_ZNSt12_Vector_baseIhSaIhEE12_Vector_implD1Ev:
.LFB9437:
	push	rbp
	.seh_pushreg	rbp
	mov	rbp, rsp
	.seh_setframe	rbp, 0
	sub	rsp, 48
	.seh_stackalloc	48
	.seh_endprologue
	mov	QWORD PTR 16[rbp], rcx
	mov	rax, QWORD PTR 16[rbp]
	mov	QWORD PTR -8[rbp], rax
	mov	rax, QWORD PTR -8[rbp]
	mov	rcx, rax
	call	_ZNSt15__new_allocatorIhED2Ev
	nop
	nop
	add	rsp, 48
	pop	rbp
	ret
	.seh_endproc
	.section	.text$_ZNSt12_Vector_baseIhSaIhEEC2EyRKS0_,"x"
	.linkonce discard
	.align 2
	.globl	_ZNSt12_Vector_baseIhSaIhEEC2EyRKS0_
	.def	_ZNSt12_Vector_baseIhSaIhEEC2EyRKS0_;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZNSt12_Vector_baseIhSaIhEEC2EyRKS0_
_ZNSt12_Vector_baseIhSaIhEEC2EyRKS0_:
.LFB9438:
	push	rbp
	.seh_pushreg	rbp
	push	rbx
	.seh_pushreg	rbx
	sub	rsp, 40
	.seh_stackalloc	40
	lea	rbp, 32[rsp]
	.seh_setframe	rbp, 32
	.seh_endprologue
	mov	QWORD PTR 32[rbp], rcx
	mov	QWORD PTR 40[rbp], rdx
	mov	QWORD PTR 48[rbp], r8
	mov	rax, QWORD PTR 32[rbp]
	mov	rdx, QWORD PTR 48[rbp]
	mov	rcx, rax
	call	_ZNSt12_Vector_baseIhSaIhEE12_Vector_implC1ERKS0_
	mov	rdx, QWORD PTR 40[rbp]
	mov	rax, QWORD PTR 32[rbp]
	mov	rcx, rax
.LEHB10:
	call	_ZNSt12_Vector_baseIhSaIhEE17_M_create_storageEy
.LEHE10:
	jmp	.L72
.L71:
	mov	rbx, rax
	mov	rax, QWORD PTR 32[rbp]
	mov	rcx, rax
	call	_ZNSt12_Vector_baseIhSaIhEE12_Vector_implD1Ev
	mov	rax, rbx
	mov	rcx, rax
.LEHB11:
	call	_Unwind_Resume
	nop
.LEHE11:
.L72:
	add	rsp, 40
	pop	rbx
	pop	rbp
	ret
	.seh_handler	__gxx_personality_seh0, @unwind, @except
	.seh_handlerdata
.LLSDA9438:
	.byte	0xff
	.byte	0xff
	.byte	0x1
	.uleb128 .LLSDACSE9438-.LLSDACSB9438
.LLSDACSB9438:
	.uleb128 .LEHB10-.LFB9438
	.uleb128 .LEHE10-.LEHB10
	.uleb128 .L71-.LFB9438
	.uleb128 0
	.uleb128 .LEHB11-.LFB9438
	.uleb128 .LEHE11-.LEHB11
	.uleb128 0
	.uleb128 0
.LLSDACSE9438:
	.section	.text$_ZNSt12_Vector_baseIhSaIhEEC2EyRKS0_,"x"
	.linkonce discard
	.seh_endproc
	.section	.text$_ZNSt12_Vector_baseIhSaIhEED2Ev,"x"
	.linkonce discard
	.align 2
	.globl	_ZNSt12_Vector_baseIhSaIhEED2Ev
	.def	_ZNSt12_Vector_baseIhSaIhEED2Ev;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZNSt12_Vector_baseIhSaIhEED2Ev
_ZNSt12_Vector_baseIhSaIhEED2Ev:
.LFB9441:
	push	rbp
	.seh_pushreg	rbp
	mov	rbp, rsp
	.seh_setframe	rbp, 0
	sub	rsp, 32
	.seh_stackalloc	32
	.seh_endprologue
	mov	QWORD PTR 16[rbp], rcx
	mov	rax, QWORD PTR 16[rbp]
	mov	rdx, QWORD PTR 16[rax]
	mov	rax, QWORD PTR 16[rbp]
	mov	rax, QWORD PTR [rax]
	sub	rdx, rax
	mov	rcx, rdx
	mov	rax, QWORD PTR 16[rbp]
	mov	rdx, QWORD PTR [rax]
	mov	rax, QWORD PTR 16[rbp]
	mov	r8, rcx
	mov	rcx, rax
	call	_ZNSt12_Vector_baseIhSaIhEE13_M_deallocateEPhy
	mov	rax, QWORD PTR 16[rbp]
	mov	rcx, rax
	call	_ZNSt12_Vector_baseIhSaIhEE12_Vector_implD1Ev
	nop
	add	rsp, 32
	pop	rbp
	ret
	.seh_handler	__gxx_personality_seh0, @unwind, @except
	.seh_handlerdata
.LLSDA9441:
	.byte	0xff
	.byte	0xff
	.byte	0x1
	.uleb128 .LLSDACSE9441-.LLSDACSB9441
.LLSDACSB9441:
.LLSDACSE9441:
	.section	.text$_ZNSt12_Vector_baseIhSaIhEED2Ev,"x"
	.linkonce discard
	.seh_endproc
	.section	.text$_ZNSt6vectorIhSaIhEE18_M_fill_initializeEyRKh,"x"
	.linkonce discard
	.align 2
	.globl	_ZNSt6vectorIhSaIhEE18_M_fill_initializeEyRKh
	.def	_ZNSt6vectorIhSaIhEE18_M_fill_initializeEyRKh;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZNSt6vectorIhSaIhEE18_M_fill_initializeEyRKh
_ZNSt6vectorIhSaIhEE18_M_fill_initializeEyRKh:
.LFB9443:
	push	rbp
	.seh_pushreg	rbp
	mov	rbp, rsp
	.seh_setframe	rbp, 0
	sub	rsp, 32
	.seh_stackalloc	32
	.seh_endprologue
	mov	QWORD PTR 16[rbp], rcx
	mov	QWORD PTR 24[rbp], rdx
	mov	QWORD PTR 32[rbp], r8
	mov	rax, QWORD PTR 16[rbp]
	mov	rcx, rax
	call	_ZNSt12_Vector_baseIhSaIhEE19_M_get_Tp_allocatorEv
	mov	rcx, rax
	mov	rax, QWORD PTR 16[rbp]
	mov	rax, QWORD PTR [rax]
	mov	r8, QWORD PTR 32[rbp]
	mov	rdx, QWORD PTR 24[rbp]
	mov	r9, rcx
	mov	rcx, rax
	call	_ZSt24__uninitialized_fill_n_aIPhyhhET_S1_T0_RKT1_RSaIT2_E
	mov	rdx, QWORD PTR 16[rbp]
	mov	QWORD PTR 8[rdx], rax
	nop
	add	rsp, 32
	pop	rbp
	ret
	.seh_endproc
	.section	.text$_ZNSt12_Vector_baseIhSaIhEE19_M_get_Tp_allocatorEv,"x"
	.linkonce discard
	.align 2
	.globl	_ZNSt12_Vector_baseIhSaIhEE19_M_get_Tp_allocatorEv
	.def	_ZNSt12_Vector_baseIhSaIhEE19_M_get_Tp_allocatorEv;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZNSt12_Vector_baseIhSaIhEE19_M_get_Tp_allocatorEv
_ZNSt12_Vector_baseIhSaIhEE19_M_get_Tp_allocatorEv:
.LFB9444:
	push	rbp
	.seh_pushreg	rbp
	mov	rbp, rsp
	.seh_setframe	rbp, 0
	.seh_endprologue
	mov	QWORD PTR 16[rbp], rcx
	mov	rax, QWORD PTR 16[rbp]
	pop	rbp
	ret
	.seh_endproc
	.section	.text$_ZNKSt6vectorIhSaIhEE11_M_data_ptrIhEEPT_S4_,"x"
	.linkonce discard
	.align 2
	.globl	_ZNKSt6vectorIhSaIhEE11_M_data_ptrIhEEPT_S4_
	.def	_ZNKSt6vectorIhSaIhEE11_M_data_ptrIhEEPT_S4_;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZNKSt6vectorIhSaIhEE11_M_data_ptrIhEEPT_S4_
_ZNKSt6vectorIhSaIhEE11_M_data_ptrIhEEPT_S4_:
.LFB9446:
	push	rbp
	.seh_pushreg	rbp
	mov	rbp, rsp
	.seh_setframe	rbp, 0
	.seh_endprologue
	mov	QWORD PTR 16[rbp], rcx
	mov	QWORD PTR 24[rbp], rdx
	mov	rax, QWORD PTR 24[rbp]
	pop	rbp
	ret
	.seh_endproc
	.section	.text$_ZNKSt6vectorIhSaIhEE5beginEv,"x"
	.linkonce discard
	.align 2
	.globl	_ZNKSt6vectorIhSaIhEE5beginEv
	.def	_ZNKSt6vectorIhSaIhEE5beginEv;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZNKSt6vectorIhSaIhEE5beginEv
_ZNKSt6vectorIhSaIhEE5beginEv:
.LFB9447:
	push	rbp
	.seh_pushreg	rbp
	mov	rbp, rsp
	.seh_setframe	rbp, 0
	sub	rsp, 16
	.seh_stackalloc	16
	.seh_endprologue
	mov	QWORD PTR 16[rbp], rcx
	mov	rax, QWORD PTR 16[rbp]
	mov	QWORD PTR -8[rbp], rax
	mov	rax, QWORD PTR -8[rbp]
	mov	rax, QWORD PTR [rax]
	mov	QWORD PTR -16[rbp], rax
	nop
	mov	rax, QWORD PTR -16[rbp]
	add	rsp, 16
	pop	rbp
	ret
	.seh_endproc
	.section	.text$_ZNKSt6vectorIhSaIhEE3endEv,"x"
	.linkonce discard
	.align 2
	.globl	_ZNKSt6vectorIhSaIhEE3endEv
	.def	_ZNKSt6vectorIhSaIhEE3endEv;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZNKSt6vectorIhSaIhEE3endEv
_ZNKSt6vectorIhSaIhEE3endEv:
.LFB9448:
	push	rbp
	.seh_pushreg	rbp
	mov	rbp, rsp
	.seh_setframe	rbp, 0
	sub	rsp, 16
	.seh_stackalloc	16
	.seh_endprologue
	mov	QWORD PTR 16[rbp], rcx
	mov	rax, QWORD PTR 16[rbp]
	add	rax, 8
	mov	QWORD PTR -8[rbp], rax
	mov	rax, QWORD PTR -8[rbp]
	mov	rax, QWORD PTR [rax]
	mov	QWORD PTR -16[rbp], rax
	nop
	mov	rax, QWORD PTR -16[rbp]
	add	rsp, 16
	pop	rbp
	ret
	.seh_endproc
	.section	.text$_ZSt5equalIN9__gnu_cxx17__normal_iteratorIPKhSt6vectorIhSaIhEEEES7_EbT_S8_T0_,"x"
	.linkonce discard
	.globl	_ZSt5equalIN9__gnu_cxx17__normal_iteratorIPKhSt6vectorIhSaIhEEEES7_EbT_S8_T0_
	.def	_ZSt5equalIN9__gnu_cxx17__normal_iteratorIPKhSt6vectorIhSaIhEEEES7_EbT_S8_T0_;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZSt5equalIN9__gnu_cxx17__normal_iteratorIPKhSt6vectorIhSaIhEEEES7_EbT_S8_T0_
_ZSt5equalIN9__gnu_cxx17__normal_iteratorIPKhSt6vectorIhSaIhEEEES7_EbT_S8_T0_:
.LFB9449:
	push	rbp
	.seh_pushreg	rbp
	mov	rbp, rsp
	.seh_setframe	rbp, 0
	sub	rsp, 80
	.seh_stackalloc	80
	.seh_endprologue
	mov	QWORD PTR 16[rbp], rcx
	mov	QWORD PTR 24[rbp], rdx
	mov	QWORD PTR 32[rbp], r8
	mov	rax, QWORD PTR 16[rbp]
	mov	QWORD PTR -8[rbp], rax
	mov	rax, QWORD PTR 24[rbp]
	mov	QWORD PTR -16[rbp], rax
	mov	rax, QWORD PTR 32[rbp]
	mov	QWORD PTR -24[rbp], rax
	mov	rax, QWORD PTR -24[rbp]
	mov	QWORD PTR -32[rbp], rax
	lea	rax, -32[rbp]
	mov	rcx, QWORD PTR [rax]
	mov	rax, QWORD PTR -16[rbp]
	mov	QWORD PTR -40[rbp], rax
	lea	rax, -40[rbp]
	mov	rdx, QWORD PTR [rax]
	mov	rax, QWORD PTR -8[rbp]
	mov	QWORD PTR -48[rbp], rax
	lea	rax, -48[rbp]
	mov	rax, QWORD PTR [rax]
	mov	r8, rcx
	mov	rcx, rax
	call	_ZSt12__equal_aux1IPKhS1_EbT_S2_T0_
	nop
	add	rsp, 80
	pop	rbp
	ret
	.seh_endproc
	.section	.text$_ZSt3minIyERKT_S2_S2_,"x"
	.linkonce discard
	.globl	_ZSt3minIyERKT_S2_S2_
	.def	_ZSt3minIyERKT_S2_S2_;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZSt3minIyERKT_S2_S2_
_ZSt3minIyERKT_S2_S2_:
.LFB9482:
	push	rbp
	.seh_pushreg	rbp
	mov	rbp, rsp
	.seh_setframe	rbp, 0
	.seh_endprologue
	mov	QWORD PTR 16[rbp], rcx
	mov	QWORD PTR 24[rbp], rdx
	mov	rax, QWORD PTR 24[rbp]
	mov	rdx, QWORD PTR [rax]
	mov	rax, QWORD PTR 16[rbp]
	mov	rax, QWORD PTR [rax]
	cmp	rdx, rax
	jnb	.L93
	mov	rax, QWORD PTR 24[rbp]
	jmp	.L94
.L93:
	mov	rax, QWORD PTR 16[rbp]
.L94:
	pop	rbp
	ret
	.seh_endproc
	.section	.text$_ZNSt6vectorIhSaIhEE11_S_max_sizeERKS0_,"x"
	.linkonce discard
	.globl	_ZNSt6vectorIhSaIhEE11_S_max_sizeERKS0_
	.def	_ZNSt6vectorIhSaIhEE11_S_max_sizeERKS0_;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZNSt6vectorIhSaIhEE11_S_max_sizeERKS0_
_ZNSt6vectorIhSaIhEE11_S_max_sizeERKS0_:
.LFB9520:
	push	rbp
	.seh_pushreg	rbp
	mov	rbp, rsp
	.seh_setframe	rbp, 0
	sub	rsp, 80
	.seh_stackalloc	80
	.seh_endprologue
	mov	QWORD PTR 16[rbp], rcx
	movabs	rax, 9223372036854775807
	mov	QWORD PTR -32[rbp], rax
	mov	rax, QWORD PTR 16[rbp]
	mov	QWORD PTR -8[rbp], rax
	mov	rax, QWORD PTR -8[rbp]
	mov	QWORD PTR -16[rbp], rax
	mov	rax, QWORD PTR -16[rbp]
	mov	QWORD PTR -24[rbp], rax
	movabs	rax, 9223372036854775807
	nop
	nop
	mov	QWORD PTR -40[rbp], rax
	lea	rdx, -40[rbp]
	lea	rax, -32[rbp]
	mov	rcx, rax
	call	_ZSt3minIyERKT_S2_S2_
	mov	rax, QWORD PTR [rax]
	add	rsp, 80
	pop	rbp
	ret
	.seh_endproc
	.section	.text$_ZNSt12_Vector_baseIhSaIhEE12_Vector_implC1ERKS0_,"x"
	.linkonce discard
	.align 2
	.globl	_ZNSt12_Vector_baseIhSaIhEE12_Vector_implC1ERKS0_
	.def	_ZNSt12_Vector_baseIhSaIhEE12_Vector_implC1ERKS0_;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZNSt12_Vector_baseIhSaIhEE12_Vector_implC1ERKS0_
_ZNSt12_Vector_baseIhSaIhEE12_Vector_implC1ERKS0_:
.LFB9526:
	push	rbp
	.seh_pushreg	rbp
	mov	rbp, rsp
	.seh_setframe	rbp, 0
	sub	rsp, 64
	.seh_stackalloc	64
	.seh_endprologue
	mov	QWORD PTR 16[rbp], rcx
	mov	QWORD PTR 24[rbp], rdx
	mov	rax, QWORD PTR 16[rbp]
	mov	QWORD PTR -8[rbp], rax
	mov	rax, QWORD PTR 24[rbp]
	mov	QWORD PTR -16[rbp], rax
	mov	rax, QWORD PTR -8[rbp]
	mov	QWORD PTR -24[rbp], rax
	mov	rax, QWORD PTR -16[rbp]
	mov	QWORD PTR -32[rbp], rax
	nop
	nop
	mov	rax, QWORD PTR 16[rbp]
	mov	rcx, rax
	call	_ZNSt12_Vector_baseIhSaIhEE17_Vector_impl_dataC2Ev
	nop
	add	rsp, 64
	pop	rbp
	ret
	.seh_endproc
	.section	.text$_ZNSt12_Vector_baseIhSaIhEE17_M_create_storageEy,"x"
	.linkonce discard
	.align 2
	.globl	_ZNSt12_Vector_baseIhSaIhEE17_M_create_storageEy
	.def	_ZNSt12_Vector_baseIhSaIhEE17_M_create_storageEy;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZNSt12_Vector_baseIhSaIhEE17_M_create_storageEy
_ZNSt12_Vector_baseIhSaIhEE17_M_create_storageEy:
.LFB9527:
	push	rbp
	.seh_pushreg	rbp
	mov	rbp, rsp
	.seh_setframe	rbp, 0
	sub	rsp, 32
	.seh_stackalloc	32
	.seh_endprologue
	mov	QWORD PTR 16[rbp], rcx
	mov	QWORD PTR 24[rbp], rdx
	mov	rdx, QWORD PTR 24[rbp]
	mov	rax, QWORD PTR 16[rbp]
	mov	rcx, rax
	call	_ZNSt12_Vector_baseIhSaIhEE11_M_allocateEy
	mov	rdx, QWORD PTR 16[rbp]
	mov	QWORD PTR [rdx], rax
	mov	rax, QWORD PTR 16[rbp]
	mov	rdx, QWORD PTR [rax]
	mov	rax, QWORD PTR 16[rbp]
	mov	QWORD PTR 8[rax], rdx
	mov	rax, QWORD PTR 16[rbp]
	mov	rdx, QWORD PTR [rax]
	mov	rax, QWORD PTR 24[rbp]
	add	rdx, rax
	mov	rax, QWORD PTR 16[rbp]
	mov	QWORD PTR 16[rax], rdx
	nop
	add	rsp, 32
	pop	rbp
	ret
	.seh_endproc
	.section	.text$_ZNSt12_Vector_baseIhSaIhEE13_M_deallocateEPhy,"x"
	.linkonce discard
	.align 2
	.globl	_ZNSt12_Vector_baseIhSaIhEE13_M_deallocateEPhy
	.def	_ZNSt12_Vector_baseIhSaIhEE13_M_deallocateEPhy;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZNSt12_Vector_baseIhSaIhEE13_M_deallocateEPhy
_ZNSt12_Vector_baseIhSaIhEE13_M_deallocateEPhy:
.LFB9528:
	push	rbp
	.seh_pushreg	rbp
	mov	rbp, rsp
	.seh_setframe	rbp, 0
	sub	rsp, 64
	.seh_stackalloc	64
	.seh_endprologue
	mov	QWORD PTR 16[rbp], rcx
	mov	QWORD PTR 24[rbp], rdx
	mov	QWORD PTR 32[rbp], r8
	cmp	QWORD PTR 24[rbp], 0
	je	.L104
	mov	rax, QWORD PTR 16[rbp]
	mov	QWORD PTR -8[rbp], rax
	mov	rax, QWORD PTR 24[rbp]
	mov	QWORD PTR -16[rbp], rax
	mov	rax, QWORD PTR 32[rbp]
	mov	QWORD PTR -24[rbp], rax
	mov	rcx, QWORD PTR -24[rbp]
	mov	rdx, QWORD PTR -16[rbp]
	mov	rax, QWORD PTR -8[rbp]
	mov	r8, rcx
	mov	rcx, rax
	call	_ZNSt15__new_allocatorIhE10deallocateEPhy
	nop
.L104:
	nop
	add	rsp, 64
	pop	rbp
	ret
	.seh_endproc
	.section	.text$_ZSt24__uninitialized_fill_n_aIPhyhhET_S1_T0_RKT1_RSaIT2_E,"x"
	.linkonce discard
	.globl	_ZSt24__uninitialized_fill_n_aIPhyhhET_S1_T0_RKT1_RSaIT2_E
	.def	_ZSt24__uninitialized_fill_n_aIPhyhhET_S1_T0_RKT1_RSaIT2_E;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZSt24__uninitialized_fill_n_aIPhyhhET_S1_T0_RKT1_RSaIT2_E
_ZSt24__uninitialized_fill_n_aIPhyhhET_S1_T0_RKT1_RSaIT2_E:
.LFB9529:
	push	rbp
	.seh_pushreg	rbp
	mov	rbp, rsp
	.seh_setframe	rbp, 0
	sub	rsp, 32
	.seh_stackalloc	32
	.seh_endprologue
	mov	QWORD PTR 16[rbp], rcx
	mov	QWORD PTR 24[rbp], rdx
	mov	QWORD PTR 32[rbp], r8
	mov	QWORD PTR 40[rbp], r9
	mov	rcx, QWORD PTR 32[rbp]
	mov	rdx, QWORD PTR 24[rbp]
	mov	rax, QWORD PTR 16[rbp]
	mov	r8, rcx
	mov	rcx, rax
	call	_ZSt20uninitialized_fill_nIPhyhET_S1_T0_RKT1_
	add	rsp, 32
	pop	rbp
	ret
	.seh_endproc
	.section	.text$_ZSt8_DestroyIPhEvT_S1_,"x"
	.linkonce discard
	.globl	_ZSt8_DestroyIPhEvT_S1_
	.def	_ZSt8_DestroyIPhEvT_S1_;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZSt8_DestroyIPhEvT_S1_
_ZSt8_DestroyIPhEvT_S1_:
.LFB9530:
	push	rbp
	.seh_pushreg	rbp
	mov	rbp, rsp
	.seh_setframe	rbp, 0
	.seh_endprologue
	mov	QWORD PTR 16[rbp], rcx
	mov	QWORD PTR 24[rbp], rdx
	nop
	pop	rbp
	ret
	.seh_endproc
	.section	.text$_ZNSt12_Vector_baseIhSaIhEE17_Vector_impl_dataC2Ev,"x"
	.linkonce discard
	.align 2
	.globl	_ZNSt12_Vector_baseIhSaIhEE17_Vector_impl_dataC2Ev
	.def	_ZNSt12_Vector_baseIhSaIhEE17_Vector_impl_dataC2Ev;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZNSt12_Vector_baseIhSaIhEE17_Vector_impl_dataC2Ev
_ZNSt12_Vector_baseIhSaIhEE17_Vector_impl_dataC2Ev:
.LFB9601:
	push	rbp
	.seh_pushreg	rbp
	mov	rbp, rsp
	.seh_setframe	rbp, 0
	.seh_endprologue
	mov	QWORD PTR 16[rbp], rcx
	mov	rax, QWORD PTR 16[rbp]
	mov	QWORD PTR [rax], 0
	mov	rax, QWORD PTR 16[rbp]
	mov	QWORD PTR 8[rax], 0
	mov	rax, QWORD PTR 16[rbp]
	mov	QWORD PTR 16[rax], 0
	nop
	pop	rbp
	ret
	.seh_endproc
	.section	.text$_ZNSt12_Vector_baseIhSaIhEE11_M_allocateEy,"x"
	.linkonce discard
	.align 2
	.globl	_ZNSt12_Vector_baseIhSaIhEE11_M_allocateEy
	.def	_ZNSt12_Vector_baseIhSaIhEE11_M_allocateEy;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZNSt12_Vector_baseIhSaIhEE11_M_allocateEy
_ZNSt12_Vector_baseIhSaIhEE11_M_allocateEy:
.LFB9603:
	push	rbp
	.seh_pushreg	rbp
	mov	rbp, rsp
	.seh_setframe	rbp, 0
	sub	rsp, 48
	.seh_stackalloc	48
	.seh_endprologue
	mov	QWORD PTR 16[rbp], rcx
	mov	QWORD PTR 24[rbp], rdx
	cmp	QWORD PTR 24[rbp], 0
	je	.L110
	mov	rax, QWORD PTR 16[rbp]
	mov	QWORD PTR -8[rbp], rax
	mov	rax, QWORD PTR 24[rbp]
	mov	QWORD PTR -16[rbp], rax
	mov	rdx, QWORD PTR -16[rbp]
	mov	rax, QWORD PTR -8[rbp]
	mov	r8d, 0
	mov	rcx, rax
	call	_ZNSt15__new_allocatorIhE8allocateEyPKv
	nop
	jmp	.L112
.L110:
	mov	eax, 0
.L112:
	add	rsp, 48
	pop	rbp
	ret
	.seh_endproc
	.section	.text$_ZSt20uninitialized_fill_nIPhyhET_S1_T0_RKT1_,"x"
	.linkonce discard
	.globl	_ZSt20uninitialized_fill_nIPhyhET_S1_T0_RKT1_
	.def	_ZSt20uninitialized_fill_nIPhyhET_S1_T0_RKT1_;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZSt20uninitialized_fill_nIPhyhET_S1_T0_RKT1_
_ZSt20uninitialized_fill_nIPhyhET_S1_T0_RKT1_:
.LFB9605:
	push	rbp
	.seh_pushreg	rbp
	mov	rbp, rsp
	.seh_setframe	rbp, 0
	sub	rsp, 48
	.seh_stackalloc	48
	.seh_endprologue
	mov	QWORD PTR 16[rbp], rcx
	mov	QWORD PTR 24[rbp], rdx
	mov	QWORD PTR 32[rbp], r8
	mov	rax, QWORD PTR 16[rbp]
	mov	QWORD PTR -16[rbp], rax
	mov	rax, QWORD PTR -16[rbp]
	mov	QWORD PTR -8[rbp], rax
	cmp	QWORD PTR 24[rbp], 0
	je	.L116
	mov	rax, QWORD PTR 32[rbp]
	movzx	eax, BYTE PTR [rax]
	movzx	eax, al
	mov	rdx, QWORD PTR 24[rbp]
	mov	rcx, QWORD PTR -8[rbp]
	mov	r8, rdx
	mov	edx, eax
	call	memset
	mov	rax, QWORD PTR 24[rbp]
	add	QWORD PTR 16[rbp], rax
.L116:
	mov	rax, QWORD PTR 16[rbp]
	add	rsp, 48
	pop	rbp
	ret
	.seh_endproc
	.section	.text$_ZSt12__equal_aux1IPKhS1_EbT_S2_T0_,"x"
	.linkonce discard
	.globl	_ZSt12__equal_aux1IPKhS1_EbT_S2_T0_
	.def	_ZSt12__equal_aux1IPKhS1_EbT_S2_T0_;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZSt12__equal_aux1IPKhS1_EbT_S2_T0_
_ZSt12__equal_aux1IPKhS1_EbT_S2_T0_:
.LFB9608:
	push	rbp
	.seh_pushreg	rbp
	mov	rbp, rsp
	.seh_setframe	rbp, 0
	sub	rsp, 48
	.seh_stackalloc	48
	.seh_endprologue
	mov	QWORD PTR 16[rbp], rcx
	mov	QWORD PTR 24[rbp], rdx
	mov	QWORD PTR 32[rbp], r8
	mov	BYTE PTR -1[rbp], 1
	mov	rcx, QWORD PTR 32[rbp]
	mov	rdx, QWORD PTR 24[rbp]
	mov	rax, QWORD PTR 16[rbp]
	mov	r8, rcx
	mov	rcx, rax
	call	_ZNSt7__equalILb1EE5equalIhEEbPKT_S4_S4_
	add	rsp, 48
	pop	rbp
	ret
	.seh_endproc
	.section	.text$_ZNSt15__new_allocatorIhE10deallocateEPhy,"x"
	.linkonce discard
	.align 2
	.globl	_ZNSt15__new_allocatorIhE10deallocateEPhy
	.def	_ZNSt15__new_allocatorIhE10deallocateEPhy;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZNSt15__new_allocatorIhE10deallocateEPhy
_ZNSt15__new_allocatorIhE10deallocateEPhy:
.LFB9641:
	push	rbp
	.seh_pushreg	rbp
	mov	rbp, rsp
	.seh_setframe	rbp, 0
	sub	rsp, 32
	.seh_stackalloc	32
	.seh_endprologue
	mov	QWORD PTR 16[rbp], rcx
	mov	QWORD PTR 24[rbp], rdx
	mov	QWORD PTR 32[rbp], r8
	mov	rdx, QWORD PTR 32[rbp]
	mov	rax, QWORD PTR 24[rbp]
	mov	rcx, rax
	call	_ZdlPvy
	nop
	add	rsp, 32
	pop	rbp
	ret
	.seh_endproc
	.section	.text$_ZNSt7__equalILb1EE5equalIhEEbPKT_S4_S4_,"x"
	.linkonce discard
	.globl	_ZNSt7__equalILb1EE5equalIhEEbPKT_S4_S4_
	.def	_ZNSt7__equalILb1EE5equalIhEEbPKT_S4_S4_;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZNSt7__equalILb1EE5equalIhEEbPKT_S4_S4_
_ZNSt7__equalILb1EE5equalIhEEbPKT_S4_S4_:
.LFB9645:
	push	rbp
	.seh_pushreg	rbp
	mov	rbp, rsp
	.seh_setframe	rbp, 0
	sub	rsp, 48
	.seh_stackalloc	48
	.seh_endprologue
	mov	QWORD PTR 16[rbp], rcx
	mov	QWORD PTR 24[rbp], rdx
	mov	QWORD PTR 32[rbp], r8
	mov	rax, QWORD PTR 24[rbp]
	sub	rax, QWORD PTR 16[rbp]
	mov	QWORD PTR -8[rbp], rax
	cmp	QWORD PTR -8[rbp], 0
	je	.L123
	mov	rcx, QWORD PTR -8[rbp]
	mov	rdx, QWORD PTR 32[rbp]
	mov	rax, QWORD PTR 16[rbp]
	mov	r8, rcx
	mov	rcx, rax
	call	_ZSt8__memcmpIhhEiPKT_PKT0_y
	test	eax, eax
	sete	al
	jmp	.L124
.L123:
	mov	eax, 1
.L124:
	add	rsp, 48
	pop	rbp
	ret
	.seh_endproc
	.section	.text$_ZNSt15__new_allocatorIhE8allocateEyPKv,"x"
	.linkonce discard
	.align 2
	.globl	_ZNSt15__new_allocatorIhE8allocateEyPKv
	.def	_ZNSt15__new_allocatorIhE8allocateEyPKv;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZNSt15__new_allocatorIhE8allocateEyPKv
_ZNSt15__new_allocatorIhE8allocateEyPKv:
.LFB9660:
	push	rbp
	.seh_pushreg	rbp
	mov	rbp, rsp
	.seh_setframe	rbp, 0
	sub	rsp, 48
	.seh_stackalloc	48
	.seh_endprologue
	mov	QWORD PTR 16[rbp], rcx
	mov	QWORD PTR 24[rbp], rdx
	mov	QWORD PTR 32[rbp], r8
	mov	rax, QWORD PTR 16[rbp]
	mov	QWORD PTR -8[rbp], rax
	movabs	rax, 9223372036854775807
	cmp	rax, QWORD PTR 24[rbp]
	setb	al
	movzx	eax, al
	test	eax, eax
	setne	al
	test	al, al
	je	.L127
	call	_ZSt17__throw_bad_allocv
.L127:
	mov	rax, QWORD PTR 24[rbp]
	mov	rcx, rax
	call	_Znwy
	nop
	add	rsp, 48
	pop	rbp
	ret
	.seh_endproc
	.section	.text$_ZSt8__memcmpIhhEiPKT_PKT0_y,"x"
	.linkonce discard
	.globl	_ZSt8__memcmpIhhEiPKT_PKT0_y
	.def	_ZSt8__memcmpIhhEiPKT_PKT0_y;	.scl	2;	.type	32;	.endef
	.seh_proc	_ZSt8__memcmpIhhEiPKT_PKT0_y
_ZSt8__memcmpIhhEiPKT_PKT0_y:
.LFB9670:
	push	rbp
	.seh_pushreg	rbp
	mov	rbp, rsp
	.seh_setframe	rbp, 0
	sub	rsp, 32
	.seh_stackalloc	32
	.seh_endprologue
	mov	QWORD PTR 16[rbp], rcx
	mov	QWORD PTR 24[rbp], rdx
	mov	QWORD PTR 32[rbp], r8
	mov	rcx, QWORD PTR 32[rbp]
	mov	rdx, QWORD PTR 24[rbp]
	mov	rax, QWORD PTR 16[rbp]
	mov	r8, rcx
	mov	rcx, rax
	call	memcmp
	add	rsp, 32
	pop	rbp
	ret
	.seh_endproc
	.def	__main;	.scl	2;	.type	32;	.endef
	.def	__gxx_personality_seh0;	.scl	2;	.type	32;	.endef
	.ident	"GCC: (x86_64-posix-seh-rev0, Built by MinGW-Builds project) 15.2.0"
	.def	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc;	.scl	2;	.type	32;	.endef
	.def	_ZNSolsEPFRSoS_E;	.scl	2;	.type	32;	.endef
	.def	_ZNSolsEi;	.scl	2;	.type	32;	.endef
	.def	RawrXD_Swarm_SendBuffer;	.scl	2;	.type	32;	.endef
	.def	RawrXD_Swarm_RecvBuffer;	.scl	2;	.type	32;	.endef
	.def	_ZNSolsEPFRSt8ios_baseS0_E;	.scl	2;	.type	32;	.endef
	.def	_ZNSolsEj;	.scl	2;	.type	32;	.endef
	.def	RawrXD_Swarm_SyncTensorShard;	.scl	2;	.type	32;	.endef
	.def	_ZNSolsEx;	.scl	2;	.type	32;	.endef
	.def	_ZNSolsEy;	.scl	2;	.type	32;	.endef
	.def	_Unwind_Resume;	.scl	2;	.type	32;	.endef
	.def	_ZSt21__glibcxx_assert_failPKciS0_S0_;	.scl	2;	.type	32;	.endef
	.def	_ZSt20__throw_length_errorPKc;	.scl	2;	.type	32;	.endef
	.def	memset;	.scl	2;	.type	32;	.endef
	.def	_ZdlPvy;	.scl	2;	.type	32;	.endef
	.def	_ZSt17__throw_bad_allocv;	.scl	2;	.type	32;	.endef
	.def	_Znwy;	.scl	2;	.type	32;	.endef
	.def	memcmp;	.scl	2;	.type	32;	.endef
	.section	.rdata$.refptr._ZSt4cerr, "dr"
	.p2align	3, 0
	.globl	.refptr._ZSt4cerr
	.linkonce	discard
.refptr._ZSt4cerr:
	.quad	_ZSt4cerr
	.section	.rdata$.refptr._ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_, "dr"
	.p2align	3, 0
	.globl	.refptr._ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
	.linkonce	discard
.refptr._ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_:
	.quad	_ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
	.section	.rdata$.refptr._ZSt4cout, "dr"
	.p2align	3, 0
	.globl	.refptr._ZSt4cout
	.linkonce	discard
.refptr._ZSt4cout:
	.quad	_ZSt4cout
