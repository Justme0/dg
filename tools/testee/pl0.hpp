#include <cstdio>
#include <cstdlib>
#include <map>
#include <queue>
#include <vector>

const int id_table_length = 100; // length of identifier table
#define nmax 14                  // max. no. of digits in numbers
#define id_length 10             // length of identifiers
#define amax 2047                // maximum address
#define levmax 3                 // maximum depth of block nesting
#define code_arr_length 2000     // size of code array

typedef long int32;
typedef unsigned long uint32;
typedef long long int64;
typedef unsigned long long uint64;

enum sym_t : uint64 {
  nul = 0x1,
  ident = 0x2,
  number = 0x4,
  plus = 0x8,
  minus = 0x10,
  times = 0x20,
  slash = 0x40,
  oddsym = 0x80,
  eql = 0x100,
  neq = 0x200,
  lss = 0x400,
  leq = 0x800,
  gtr = 0x1000,
  geq = 0x2000,
  lparen = 0x4000,
  rparen = 0x8000,
  comma = 0x10000,
  semicolon = 0x20000,
  period = 0x40000,
  becomes = 0x80000,
  beginsym = 0x100000,
  endsym = 0x200000,
  ifsym = 0x400000,
  thensym = 0x800000,
  whilesym = 0x1000000,
  dosym = 0x2000000,
  callsym = 0x4000000,
  constsym = 0x8000000,
  varsym = 0x10000000,
  procsym = 0x20000000,
  elsesym = 0x40000000,
  tillsym = 0x80000000,
  exitsym = 0x100000000,
  writesym = 0x200000000,
  readsym = 0x400000000,
  realsym = 0x800000000,
  andsym = 0x1000000000,
  boolsym = 0x2000000000,
  arraysym = 0x4000000000,
  divsym = 0x8000000000,
  truesym = 0x10000000000,
  falsesym = 0x20000000000,
  intsym = 0x40000000000,
  funcsym = 0x80000000000,
  modsym = 0x100000000000,
  notsym = 0x200000000000,
  ofsym = 0x400000000000,
  orsym = 0x800000000000,
  typesym = 0x1000000000000,
  lsbra = 0x2000000000000,
  rsbra = 0x4000000000000,
  colon = 0x8000000000000,
};

enum enter_type {
  int_const,
  real_const,
  int_var,
  real_var,
  bool_var,
  array_var,
  array_define,
  procedure,
  function,
};

typedef enum { integer = 1, real = 2, boolean = 3, array = 4 } type;

typedef enum {
  lit = 1,
  opr,
  lod,
  sto,
  cal,
  Int,
  jmp,
  jpc,
  cst,
  pyp,
  iar,
  pop,
  psh,
  opt,
  ipt,
  arr,
  rhp,
  whp,
  hlt
} fct;

typedef struct {
  fct f;   // function code
  int32 l; // level
  int32 a; // displacement address
} instruction;

/*  lit 0, a : load int_const a
        opr 0, a : execute operation a
        lod l, a : load int_var l, a
        sto l, a : store int_var l, a
        cal l, a : call procedure a at level l
        Int 0, a : increment t-register by a
        jmp 0, a : jump to a
        jpc 0, a : jump conditional to a
        cst _, _ : cast int to real
        pyp _, a : push type specifier
                           0 means int, 1 means real, 2 means bool
        iar _, a : initialize an array
                           a means the depth of array
        pop l, a : l means the reg's type, a means the serial num.
        psh l, a : l means the reg's type, a means the serial num.
        aca _, a : access an item of an array of depth a
        wtm _, _ : write to memory. [t] is type, [t-1] is value, [t-2] is addr
        opt _, a : write to standard output, a means type
        ipt _, a : read from standard input, and place it on the stack top. a
   means type
        arr l, a : access an array in level l and offset a
                           push the addr of target item in stack top
                           together with the array's type
        rhp _, _ : the type id is on the stack top
                           the memory address is under the type id
        whp _, _ : just like the rhp
        hlt _, _ : halt the running of the intepreter
                           */

typedef const int32 op_type;

op_type op_ret = 0;
op_type op_neg = 1;
op_type op_plus = 2;
op_type op_minus = 3;
op_type op_times = 4;
op_type op_slash = 5;
op_type op_odd = 6;
op_type op_eq = 8;
op_type op_neq = 9;
op_type op_lt = 10;
op_type op_ge = 11;
op_type op_gt = 12;
op_type op_le = 13;
op_type op_not = 14;
op_type op_and = 15;
op_type op_or = 16;
op_type op_div = 17;
op_type op_mod = 18;

char ch;   // last character read
sym_t sym; // last symbol read
// char id[id_length + 1]; // last identifier read
std::string last_id;
int32 last_int;   // last number read
double last_real; // last real read
bool last_bool;

int32 charCount;  // character count
int32 lineLength; // line length
int32 kk, err;
int32 cx; // code allocation index
type last_type;

char line[81];
char a[id_length + 1];
instruction code[code_arr_length + 1];
sym_t ssym[256];

char mnemonic[10][3 + 1];
// sym_t decls_sys, statement_sys, factor_sys;

uint64 decls_sys = constsym | varsym | typesym | procsym | funcsym;
uint64 statement_sys =
    beginsym | callsym | ifsym | whilesym | readsym | writesym | exitsym;
uint64 factor_sys =
    ident | number | lparen | notsym | truesym | falsesym | oddsym;
uint64 termsys = factor_sys;
uint64 simpExpsys = termsys | minus | plus;
uint64 expression_sys = simpExpsys;
uint64 block_sym = endsym;

struct {
  std::string name;
  enum enter_type kind;
  int32 val;
  int32 level;
  int32 addr;
} table[id_table_length + 1];

char infilename[80];
FILE *infile;

// the following variables for block
int32 dx;  // data allocation index
int32 lev; // current depth of block nesting
int32 tx;  // current table index

// the following array space for interpreter
#define stacksize 50000
int32 s[stacksize]; // datastore

std::queue<sym_t> cached_symbols;
std::queue<char> cached_chars;

inline void enqueue_symbol(sym_t symbol) { cached_symbols.push(symbol); }

inline sym_t dequeue_symbol() {
  auto m = cached_symbols.front();
  cached_symbols.pop();
  return m;
}

typedef union {
  struct {
    int32 fst;
    int32 snd;
  };
  double d;
} dni;

inline dni resolve_double(double d) { return *reinterpret_cast<dni *>(&d); }

inline double recover_double(void *addr) {
  auto d = *static_cast<double *>(addr);
  return d;
}

inline double regain_double(int32 fst, int32 snd) {
  dni dp;
  dp.fst = fst;
  dp.snd = snd;
  return dp.d;
}

inline double regain_double2(int32 *addr) {
  return regain_double(*addr, *(addr + 1));
}
inline void writeDouble(double d, int32 *addr) {
  dni dp;
  dp.d = d;
  addr[0] = dp.fst;
  addr[1] = dp.snd;
}

typedef struct {
  int32 upper_bound;
  int32 lower_bound;
  type item_type;
  int32 item_id;
} array_decl;

std::vector<array_decl> array_decls;
std::map<std::string, uint32> array_names;
std::vector<double> reals;

class NotImplementedException {};

std::map<std::string, sym_t> reservedWords;

enum funcType { integer_ft, real_ft, boolean_ft, proc_ft };

std::vector<std::pair<funcType, std::vector<type>>> func_decls;

typedef const int32 error_id;
error_id error_needs_interupt = 0;
error_id error_unknown_ident = 166;
error_id error_unexpected_token = 167;
error_id error_mix_becomes_equal = 168;
error_id error_array_as_argument = 169;
error_id error_type_unmatch = 170;
error_id error_braket_unmatch = 171;
error_id error_incorrect_type = 172;
error_id error_const_assign = 173;
error_id error_duplicate_ident = 174;
error_id error_invalid_array_bound = 175;

const sym_t fsys_expression = nul;

/*
opr 0  : return
opr 1  : neg
opr 2  : plus
opr 3  : minus
opr 4  : mul
opr 5  : float div
opr 6  : is odd
opr 8  : eq
opr 9  : neq
opr 10 : lt
opr 11 : ge
opr 12 : gt
opr 13 : le
opr 14 : not
opr 15 : and
opr 16 : or
opr 17 : integer div
opr 18 : mod

*/

int showSymbol(sym_t s);

class yyArray {
public:
  class ArrayBound {
  public:
    int begin, width;
    ArrayBound(int begin, int width) {
      this->begin = begin;
      this->width = width;
    }
  };
  type array_type;
  std::vector<ArrayBound> bounds;
  void *items;
};

const int32 ipt_true = 1;
const int32 ipt_false = 0;

const int32 proc_return_specifier = 5;
