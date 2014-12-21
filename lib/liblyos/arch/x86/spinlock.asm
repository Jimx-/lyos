;    This file is part of Lyos.
;
;    Lyos is free software: you can redistribute it and/or modify
;    it under the terms of the GNU General Public License as published by
;    the Free Software Foundation, either version 3 of the License, or
;    (at your option) any later version.

;    Lyos is distributed in the hope that it will be useful,
;    but WITHOUT ANY WARRANTY; without even the implied warranty of
;    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;    GNU General Public License for more details.
;
;    You should have received a copy of the GNU General Public License
;    along with Lyos.  If not, see <http://www.gnu.org/licenses/>.

bits 32
[section .text]

global  arch_spinlock_lock
global  arch_spinlock_unlock

; ========================================================================
;                  void arch_spinlock_lock(u32 * lock);
; ========================================================================
arch_spinlock_lock:
	mov	eax, [esp + 4]
	mov	edx, 1
.2:
	mov	ecx, 1
	xchg ecx, [eax]
	test ecx, ecx
	je .0

	cmp	edx, 1 << 16
	je	.1
	shl	edx, 1
.1:
	mov	ecx, edx
.3:
	pause
	sub ecx, 1
	test ecx, ecx
	jz .2
	jmp	.3
.0:
	mfence
	ret

; ========================================================================
;                  void arch_spinlock_unlock(u32 * lock);
; ========================================================================
arch_spinlock_unlock:
	mov	eax, [esp + 4]
	mov	ecx, 0
	xchg [eax], ecx
	mfence
	ret
