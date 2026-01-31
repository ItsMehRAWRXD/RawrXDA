; Quantum Hello World - n0mn0m Quantum ASM
; This creates a quantum superposition and measures it

; Initialize quantum circuit
qreg q[2]     ; 2 qubits
creg c[2]     ; 2 classical bits for measurement

; Create quantum superposition
h q[0]        ; Hadamard gate on qubit 0
h q[1]        ; Hadamard gate on qubit 1

; Create entanglement
cx q[0], q[1] ; CNOT gate (entangles qubits)

; Add some quantum operations
x q[0]        ; Pauli-X (quantum NOT)
y q[1]        ; Pauli-Y gate
z q[0]        ; Pauli-Z gate

; Custom rotation
rx(pi/4) q[0] ; Rotate around X-axis by π/4
ry(pi/2) q[1] ; Rotate around Y-axis by π/2

; Measure quantum states
measure q[0] -> c[0]  ; Measure qubit 0
measure q[1] -> c[1]  ; Measure qubit 1

; This creates a quantum "Hello World" that demonstrates:
; - Quantum superposition
; - Quantum entanglement
; - Quantum gates
; - Quantum measurement
