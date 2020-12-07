

#ifndef MDZ80_H_
#define MDZ80_H_

/* If you comment out below semantic , this program outputs English Messages. */
// #define MESSAGETYPE_JAPANESE

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned long ulong;

extern int disasmZ80( uchar *buf, ulong base, ulong addr, char *stream, int sw, int nullcheck, int jsonFormat, uint32_t max_stream_len );

typedef struct _regZ80 {
	uchar	a[2];
	uchar	b[2];
	uchar	c[2];
	uchar	d[2];
	uchar	e[2];
	uchar	h[2];
	uchar	l[2];
	ushort	ix;
	ushort	iy;
	union {
		uchar f;
		struct {
			uchar carry : 1;
			uchar zero : 1;
			uchar intr : 1;
			uchar dec : 1;
			uchar brk : 1;
			uchar resv : 1;
			uchar ovfl : 1;
			uchar sign : 1;
		};
	} f[2];
	ushort sp;
	ushort pc;
} regZ80;

typedef struct _memargstr {
	ushort staddr;				// �J�n�A�h���X
	ushort edaddr;				// �I���A�h���X
	int access;					// �A�N�Z�X���@�̃t���O
	char *comment;				// �R�����g�̓�e
} memargstr;

typedef struct {
	int		len;				// ���ߒ�
	int		index;				// �I�y�����h���Q�ƊJ�n����f�[�^�̈ʒu
	int		nimonic;			// �j�[���j�b�N������̔ԍ�
	int		op_num;				// �I�y�����h��
	int		op0_type;			// ��1�I�y�����h
	int		op1_type;			// ��2�I�y�����h
	int		acc_type;			// �A�N�Z�X�^�C�v(���[�h/���C�g/�C��/�A�E�g)
} disZ80data;

// dis6502str.type = Address Type | Access Type;
// Address Type
enum {
	OT_NONE = 0,

	OT_REG_A,			// A Register
	OT_REG_B,			// B Register
	OT_REG_C,			// C Register
	OT_REG_D,			// D Register
	OT_REG_E,			// E Register
	OT_REG_H,			// H Register
	OT_REG_L,			// L Register
	OT_REG_F,			// F Register
	OT_REG_I,			// Interrupt Vector
	OT_REG_R,			// Refresh Counter

	OT_REG_AF,			// AF  Register
	OT_REG_RAF,			// AF' Register
	OT_REG_BC,			// BC  Register
	OT_REG_DE,			// DE  Register
	OT_REG_HL,			// HL  Register
	OT_REG_IX,			// HL  Register
	OT_REG_IXL,			// HL  Register
	OT_REG_IXH,			// HL  Register
	OT_REG_IY,			// HL  Register
	OT_REG_IYL,			// HL  Register
	OT_REG_IYH,			// HL  Register
	OT_REG_SP,			// SP  Register

	OT_PORT_C,			// �|�[�g		(C)

	OT_ABS_BC,			// ��΃A�h���X	(BC)
	OT_ABS_DE,			// ��΃A�h���X	(DE)
	OT_ABS_HL,			// ��΃A�h���X	(HL)
	OT_ABS_IX,			// ��΃A�h���X	(IX)
	OT_ABS_IY,			// ��΃A�h���X	(IY)
	OT_ABS_SP,			// ��΃A�h���X	(SP)

	OT_NZ,				// �R���f�B�V���� non zero
	OT_Z,				// �R���f�B�V���� zero
	OT_NC,				// �R���f�B�V���� non carry
	OT_C,				// �R���f�B�V���� carry
	OT_PO,				// �R���f�B�V���� parity odd
	OT_PE,				// �R���f�B�V���� parity even
	OT_P,				// �R���f�B�V���� sign positive(plus)
	OT_M,				// �R���f�B�V���� sign negative(minaus)

	OT_BIT_0,			// bit����p
	OT_BIT_1,			// bit����p
	OT_BIT_2,			// bit����p
	OT_BIT_3,			// bit����p
	OT_BIT_4,			// bit����p
	OT_BIT_5,			// bit����p
	OT_BIT_6,			// bit����p
	OT_BIT_7,			// bit����p

	OT_IM_0,			// IM	0
	OT_IM_1,			// IM	1
	OT_IM_2,			// IM	2
/* hex byte data */
	OT_IMM_BYTE = 0x0100,	// ���l			d8
	OT_RST,					// rst
	OT_IMM_PORT,			// �|�[�g		(d8)
	OT_UND,					// ����`���ߗp1byte
	OT_NUL,					// �A�������f�[�^�̏��������p
/* hex byte data(byte offset) */
	OT_IDX_IX	= 0x0200,	// IX�Ԑ�		(IX+d8)
	OT_IDX_IY,				// IY�Ԑ�		(IX+d8)
/* hex word data(byte offset) */
	OT_REL		= 0x0400,	// ���΃A�h���X	d8
/* hex word data	*/
	OT_IMM_WORD = 0x0800,	// ���l			d16
	OT_ABS,					// ��΃A�h���X	(d16)
};

#define ADT_MASK 0xff

// Access Type
#define	ACT_NL	0
#define	ACT_RD	1
#define	ACT_WT	2
#define	ACT_OT	4
#define	ACT_IN	8
#define	ACT_AD	0x4000
#define	ACT_CL	0x8000

#define ACT_RW ( ACT_RD | ACT_WT )
#define ACT_IO ( ACT_IN | ACT_OT )

// Name Type
enum {
	NMT_UND = 0,
	NMT_NUL,
	NMT_LD,
	NMT_PUSH,
	NMT_POP,
	NMT_EX,
	NMT_EXX,
	NMT_LDI,
	NMT_LDIR,
	NMT_LDD,
	NMT_LDDR,
	NMT_CPI,
	NMT_CPIR,
	NMT_CPD,
	NMT_CPDR,
	NMT_ADD,
	NMT_ADC,
	NMT_SUB,
	NMT_SBC,
	NMT_AND,
	NMT_OR,
	NMT_XOR,
	NMT_CP,
	NMT_INC,
	NMT_DEC,
	NMT_DAA,
	NMT_CPL,
	NMT_NEG,
	NMT_CCF,
	NMT_SCF,
	NMT_NOP,
	NMT_HALT,
	NMT_DI,
	NMT_EI,
	NMT_IM,
	NMT_RLCA,
	NMT_RLA,
	NMT_RRCA,
	NMT_RRA,
	NMT_RLC,
	NMT_RL,
	NMT_RRC,
	NMT_RR,
	NMT_SLA,
	NMT_SLL,
	NMT_SRA,
	NMT_SRL,
	NMT_RLD,
	NMT_RRD,
	NMT_BIT,
	NMT_SET,
	NMT_RES,
	NMT_JP,
	NMT_JR,
	NMT_DJNZ,
	NMT_CALL,
	NMT_RET,
	NMT_RETI,
	NMT_RETN,
	NMT_RST,
	NMT_IN,
	NMT_INI,
	NMT_INIR,
	NMT_IND,
	NMT_INDR,
	NMT_OUT,
	NMT_OUTI,
	NMT_OTIR,
	NMT_OUTD,
	NMT_OTDR,
};

enum {
	INTEL,
	MOTOROLA,
	C_LANG,
	JUSTHEX,
};

#endif/*MDZ80_H_*/
