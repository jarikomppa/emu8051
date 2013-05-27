struct em8051;

// Operation: returns number of ticks the operation should take
typedef int (*em8051operation)(struct em8051 *aCPU); 

// Decodes opcode at position, and fills the buffer with the assembler code. 
// Returns how many bytes the opcode takes.
typedef int (*em8051decoder)(struct em8051 *aCPU, int aPosition, char *aBuffer);

// Callback: some exceptional situation occurred. See EM8051_EXCEPTION enum, below
typedef void (*em8051exception)(struct em8051 *aCPU, int aCode);

// Callback: an SFR register is about to be read (not called for 'a' ops nor psw changes)
// Default is to return the value in the SFR register. Ports may act differently.
typedef int (*em8051sfrread)(struct em8051 *aCPU, int aRegister);

// Callback: an SFR register has changed (not called for 'a' ops)
// Default is to do nothing
typedef void (*em8051sfrwrite)(struct em8051 *aCPU, int aRegister);


struct em8051
{
    unsigned char *mCodeMem; // 1k - 64k, must be power of 2
    int mCodeMemSize; 
    unsigned char *mExtData; // 0 - 64k, must be power of 2
    int mExtDataSize;
    unsigned char *mLowerData; // 128 bytes
    unsigned char *mUpperData; // 0 or 128 bytes; leave to NULL if none
    unsigned char *mSFR; // 128 bytes; (special function registers)
    int mPC; // Program Counter; outside memory area
    int mTickDelay; // How many ticks should we delay before continuing
    em8051operation op[256]; // function pointers to opcode handlers
    em8051decoder dec[256]; // opcode-to-string decoder handlers    
    em8051exception except; // callback: exceptional situation occurred
    em8051sfrread sfrread; // callback: SFR register being read
    em8051sfrwrite sfrwrite; // callback: SFR register written
};

// set the emulator into reset state. Must be called before tick(), as
// it also initializes the function pointers.
void reset(struct em8051 *aCPU);

// run one emulator tick, or 12 hardware clock cycles.
// returns 1 if a new operation was executed.
int tick(struct em8051 *aCPU);

// decode the next operation as character string.
// buffer must be big enough (64 bytes is very safe). 
// Returns length of opcode.
int decode(struct em8051 *aCPU, int aPosition, unsigned char *aBuffer);

// SFR register locations
enum SFR_REGS
{
    REG_ACC = 0xE0 - 0x80,
    REG_B   = 0xF0 - 0x80,
    REG_PSW = 0xD0 - 0x80,
    REG_SP  = 0x81 - 0x80,
    REG_DPL = 0x82 - 0x80,
    REG_DPH = 0x83 - 0x80,
    REG_P0  = 0x80 - 0x80,
    REG_P1  = 0x90 - 0x80,
    REG_P2  = 0xA0 - 0x80,
    REG_P3  = 0xB0 - 0x80,
    REG_IP  = 0xB8 - 0x80,
    REG_IE  = 0xA8 - 0x80,
    REG_TIMOD = 0x89 - 0x80,
    REG_TCON = 0x88 - 0x80,
    REG_TH0 = 0x8C - 0x80,
    REG_TL0 = 0x8A - 0x80,
    REG_TH1 = 0x8D - 0x80,
    REG_TL1 = 0x8B - 0x80,
    REG_SCON = 0x98 - 0x80,
    REG_PCON = 0x87 - 0x80
};

enum PSW_BITS
{
    PSW_P = 0,
    PSW_UNUSED = 1,
    PSW_OV = 2,
    PSW_RS0 = 3,
    PSW_RS1 = 4,
    PSW_F0 = 5,
    PSW_AC = 6,
    PSW_C = 7
};

enum PSW_MASKS
{
    PSWMASK_P = 0x01,
    PSWMASK_UNUSED = 0x02,
    PSWMASK_OV = 0x04,
    PSWMASK_RS0 = 0x08,
    PSWMASK_RS1 = 0x10,
    PSWMASK_F0 = 0x20,
    PSWMASK_AC = 0x40,
    PSWMASK_C = 0x80
};

enum EM8051_EXCEPTION
{
    EXCEPTION_STACK,  // stack address > 127
    EXCEPTION_ACC_TO_A, // acc-to-a move operation; illegal (acc-to-acc is ok, a-to-acc is ok..)
    EXCEPTION_IRET_PSW_MISMATCH, // psw not preserved over interrupt call (doesn't care about P, F0 or UNUSED)
    EXCEPTION_IRET_SP_MISMATCH,  // sp not preserved over interrupt call
    EXCEPTION_ILLEGAL_OPCODE     // for the single 'reserved' opcode in the architecture
};