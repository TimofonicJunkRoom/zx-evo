
;---------------------------------------
; Wild Commander plugin header
;---------------------------------------
		; ��������� (���������) �������
		ds	16					; +00 reserved
		db	"WildCommanderMDL"			; +16 WildCommanderMDL
		db	#07					; +32 ������ �������
		db 	#00					; +33 reserved
		db	#01					; +34 ���������� �������
		db	#00					; +35 ����� ��������, ������� ����� ���������� � #8000

; +36 +0 - ����� ��������, +1 - ������ ����� (512b)
		db	#00, (mainEnd - mainStart) / 512 + 1
		db	#00,#00
		db	#00,#00
		db	#00,#00
		db	#00,#00
		db	#00,#00

		ds	16					; +48 reserved

; +64 ���� ����������:
    db  "JPG"
    db  "PNG"
    db  "AVI"
    db  "DLS"
		ds	28 * 3
    
		db	#00					; +160 #00

; +161 ������������ ������ ����� (����������, ����� ����� �� ����� ����������)
		dw	#FFFF, #FFFF

; +165 ��� �������
		db	"FT812 Media Viewer (c)2016 TSL  "

; +197 ��� ������� ��� ������� ������ ���������� ������:
		db #00
								; 	   #00 - ������ �� ����������
								; 	   #01 - ����� ����� �������� �������
								; 	   #02 - �� ������� ������������ � �� ����
								; 	   #03 - �� ���� �������
