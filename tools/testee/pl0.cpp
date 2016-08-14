// pl/0 compiler with code generation
#include "pl0.hpp"
#include <cassert>
#include <cstring>
#include <memory>
#include <stack>

using std::vector;
using std::pair;
using std::make_shared;
using std::make_tuple;
using std::make_pair;
using std::shared_ptr;
using std::string;
using std::stack;

void error(error_id n) {
  int32 i;

  printf(" ****");
  for (i = 1; i < charCount; i++) {
    printf(" ");
  }
  printf("^%2ld\n", n);
  err++;
  if (n == error_needs_interupt)
    assert(0);
}

void getch() {
  if (!cached_chars.empty()) {
    ch = cached_chars.front();
    cached_chars.pop();
    return;
  }
  if (charCount == lineLength) { //
    if (feof(infile)) {
      printf("************************************\n");
      printf("      program incomplete\n");
      printf("************************************\n");
      getchar();
      exit(1);
    }
    lineLength = 0;
    charCount = 0;
    printf("%5ld ", cx);
    while (!feof(infile) && (ch = getc(infile)) != '\n') {
      printf("%c", ch);
      lineLength = lineLength + 1;
      line[lineLength] = ch;
    }
    printf("\n");
    lineLength = lineLength + 1;
    line[lineLength] = ' ';
  }
  charCount = charCount + 1;
  ch = line[charCount];
}

int isAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

int isDigit(char c) { return c >= '0' && c <= '9'; }

void getch2() {
  while (ch == ' ' || ch == '\t')
    getch();
}

void getNumber() {
  auto hasTail = false, isReal = false;
  int il;
  double rl;
  std::vector<char> buffer{};
  while (isDigit(ch)) {
    buffer.push_back(ch);
    getch();
  }
  if (ch == '.') {
    getch();
    if (isDigit(ch)) {
      buffer.push_back('.');
      while (isDigit(ch)) {
        buffer.push_back(ch);
        getch();
      }
      isReal = true;
    } else if (ch == '.') {
      hasTail = true;
      getch();
    } else
      error(3);
  }

  sym = number;
  if (isReal) {
    if (sscanf(&buffer[0], "%lf", &rl) == 1) {
      last_type = real;
      last_real = rl;
    } else
      error(3);
  } else {
    if (sscanf(&buffer[0], "%d", &il) == 1) {
      last_int = il;
      last_type = integer;
    } else
      error(3);
    if (hasTail)
      enqueue_symbol(tillsym);
  }
}

void getSymbol() {
  if (!cached_symbols.empty()) {
    sym = dequeue_symbol();
    return;
  }

  getch2();

  if (isAlpha(ch)) {
    vector<char> buffer;
    buffer.push_back(ch);
    getch();
    while (isAlpha(ch) || isDigit(ch)) {
      buffer.push_back(ch);
      getch();
    }
    if (buffer.size() > id_length)
      buffer.erase(buffer.end() - (buffer.size() - id_length), buffer.end());
    string curr_id(buffer.data(), buffer.size());
    auto find_result = reservedWords.find(curr_id);
    if (find_result == reservedWords.end()) {
      sym = ident;
      last_id = curr_id;
    } else
      sym = find_result->second;
  } else if (isDigit(ch))
    getNumber();
  else if (ch == ':') {
    getch();
    if (ch == '=') {
      sym = becomes;
      getch();
    } else {
      sym = colon;
    }
  } else if (ch == '<') {
    getch();
    if (ch == '=') {
      sym = leq;
      getch();
    } else if (ch == '>') {
      sym = neq;
      getch();
    } else {
      sym = lss;
    }
  } else if (ch == '>') {
    getch();
    if (ch == '=') {
      sym = geq;
      getch();
    } else {
      sym = gtr;
    }
  } else if (ch == '/') // get comments
  {
    getch();
    if (ch == '*') {
      getch();
      //			while (ch != '*')
      //			{
      //				getch();
      //				if (ch == '/') break;
      //			}
      while (true) {
        if (ch == '*') {
          getch();
          if (ch == '/')
            break;
          getch();
        } else
          getch();
      }
      getch();
      getSymbol();
    } else {
      sym = ssym[static_cast<size_t>('/')];
      cached_chars.push(ch);
      getch();
    }
  } else if (ch == '.') {
    getch();
    if (ch == '.') {
      sym = tillsym;
      getch();
    } else
      sym = period;
  } else {
    sym = ssym[static_cast<size_t>(ch)];
    getch();
  }
}

void gen(fct f, int32 l, int32 a) {
  if (cx > code_arr_length) {
    printf("program too long\n");
    exit(1);
  }
  code[cx].f = f;
  code[cx].l = l;
  code[cx].a = a;
  cx += 1;
}

void test(uint64 s1, uint64 s2, int32 n) {
  if (!(sym & s1)) {
    error(n);
    s1 = s1 | s2;
    while (!(sym & s1)) {
      getSymbol();
    }
  }
}

int32 position(string id) {
  for (auto i = tx; i > 0; i -= 1)
    if (table[i].name == id)
      return i;
  return 0;
}

void enter(enum enter_type k) { // enter enter_type into table
  auto posi = position(last_id);
  if (posi == 0) {
  } else {
    auto entry = table[posi];
    if (entry.level == lev) {
      error(error_duplicate_ident);
      return;
    }
  }

  tx += 1;
  table[tx].name = last_id;
  table[tx].kind = k;
  switch (k) {
  case int_const:
    if (last_int > amax) {
      error(31);
      last_int = 0;
    }
    table[tx].val = last_int;
    break;
  case real_const:
    reals.push_back(last_real);
    table[tx].addr = reals.size() - 1;
    break;
  case int_var:
    table[tx].level = lev;
    table[tx].addr = dx;
    dx = dx + 1;
    break;
  case real_var:
    table[tx].level = lev;
    table[tx].addr = dx;
    dx += 2;
    break;
  case bool_var:
    table[tx].level = lev;
    table[tx].addr = dx;
    dx += 1;
    break;
  case procedure:
  case function:
    table[tx].level = lev;
    table[tx].val = func_decls.size() - 1;
    break;
  case array_var:
    table[tx].level = lev;
    table[tx].addr = dx;
    //		table[tx].val is the position of type description in array_decls
    //		defined in varDecl
    dx += 1;
    break;
  case array_define:
    table[tx].level = lev;
    table[tx].addr = array_decls.size() - 1;
    break;
  }
}

inline void pyp_int() { gen(pyp, 0, integer); }
inline void pyp_real() { gen(pyp, 0, real); }
inline void pyp_bool() { gen(pyp, 0, boolean); }

void push_type(type t) {
  switch (t) {
  case integer:
    pyp_int();
    break;
  case real:
    pyp_real();
    break;
  case boolean:
    pyp_bool();
    break;
  default:
    error(0);
  }
}

void constDefine() {
  if (sym == ident) {
    getSymbol(); // '=' or '=='
    if (sym == eql || sym == becomes) {
      if (sym == becomes)
        error(9);
      getSymbol();
      if (sym == number) {
        switch (last_type) {
        case integer:
          enter(int_const);
          break;
        case real:
          enter(real_const);
          break;
        default:
          error(1);
          break;
        }
        getSymbol();
        if (sym == semicolon)
          getSymbol();
        else
          error(18);
      } else
        error(3);
    } else
      error(error_unexpected_token);
  } else
    error(4);
}

void constDeclaration() {
  if (sym == constsym) {
    getSymbol();
    while (sym == ident) {
      constDefine();
    }
  }
}

void typeDefine() {
  if (sym == ident) {
    auto type_name = last_id;
    getSymbol();
    if (sym == eql || sym == becomes) {
      if (sym == becomes)
        error(error_mix_becomes_equal);
      getSymbol();
      type bottom;
      std::stack<pair<int32, int32>> decls;

      if (sym != arraysym) {
        error(43);
      }

      while (sym == arraysym) {
        int32 upper, lower;
        getSymbol();
        if (sym == lsbra) {
          getSymbol();
          if (sym == number && last_type == integer) {
            lower = last_int;
            getSymbol();
            if (sym == tillsym) {
              getSymbol();
              if (sym == number && last_type == integer) {
                upper = last_int;
                if (upper < lower)
                  error(error_invalid_array_bound);
                getSymbol();
                if (sym == rsbra) {
                  getSymbol();
                  if (sym == ofsym) {
                    getSymbol();
                    decls.push(make_pair(lower, upper));
                    if (sym == arraysym)
                      continue;
                    if (sym == intsym)
                      bottom = integer;
                    else if (sym == realsym)
                      bottom = real;
                    else if (sym == boolsym)
                      bottom = boolean;
                    else {
                      error(5);
                      // printf("not an expected symbol\n");
                      return;
                    }
                    getSymbol();
                    if (sym != semicolon)
                      error(18);

                    auto sign1 = true;
                    while (!decls.empty()) {
                      auto p = decls.top();
                      decls.pop();
                      array_decl decl;
                      decl.upper_bound = p.second;
                      decl.lower_bound = p.first;
                      if (sign1) {
                        decl.item_type = bottom;
                        sign1 = false;
                      } else {
                        decl.item_type = array;
                        decl.item_id = array_decls.size() - 1;
                      }
                      array_decls.push_back(decl);
                    }
                    last_id = type_name;
                    enter(array_define);
                    getSymbol();
                    return;
                  }
                  error(47);
                } else
                  error(51);
              } else {
                if (sym != number)
                  error(3);
                else if (last_type != integer)
                  error(62);
              }
            } else
              error(32);
          } else {
            if (sym != number)
              error(3);
            else if (last_type != integer)
              error(62);
          }
        } else
          error(50);
      }
    }
  } else
    error(0);
}

void typeDeclaration() {
  if (sym == typesym) {
    getSymbol();
    while (sym == ident)
      typeDefine();
  }
}

void varDefine(vector<pair<int32, int32>> &arrays) {
  if (sym == ident) {
    vector<string> vars;
    vars.push_back(last_id);
    getSymbol();

    while (sym == comma) {
      getSymbol();
      if (sym == ident) {
        vars.push_back(last_id);
        getSymbol();
      }
    }
    if (sym == colon) {
      getSymbol();
      enter_type line_type;
      string arr_type_name;
      if (sym == intsym)
        line_type = int_var;
      else if (sym == realsym)
        line_type = real_var;
      else if (sym == boolsym)
        line_type = bool_var;
      else if (sym == ident) {
        line_type = array_var;
        arr_type_name = last_id;

      } // array init!
      else {
        error(101);
        do {
          getSymbol();
        } while (!(sym & (procsym | funcsym | beginsym)));
        return;
      }

      for (auto v : vars) {
        last_id = v;
        enter(line_type);
        auto arr_index = tx;
        if (line_type == array_var) {
          auto decl_index = position(arr_type_name);
          if (decl_index == 0) {
            error(error_unknown_ident);
            return;
          } else if (table[decl_index].kind != array_define) {
            error(error_unexpected_token);
            return;
          } else { // array initialization
            arrays.push_back(make_pair(arr_index, decl_index));

            //						auto type_define_item =
            // table[i];
            //						auto& curr_table_item =
            // table[tx];
            //						auto decl_num =
            // type_define_item.addr;
            //						curr_table_item.val =
            // decl_num;
            //						auto push_arr_info = [=](int start,
            //int
            // width)
            //						{
            //							gen(lit, 0,
            // start);
            //							gen(lit, 0,
            // width);
            //						};
            //
            //						stack<array_decl> decls;
            //						type bottom_type;
            //
            //						while (true)
            //						{
            //							auto curr_decl =
            // array_decls[decl_num];
            //							decls.push(curr_decl);
            //							if (curr_decl.item_type
            //==
            // array)
            //								decl_num
            //=
            // curr_decl.item_id;
            //							else
            //							{
            //								bottom_type
            //=
            // curr_decl.item_type;
            //								break;
            //							}
            //						}
            //						push_type(bottom_type);
            //						auto depth =
            // decls.size();
            //						while (!decls.empty())
            //						{
            //							auto curr_decl =
            // decls.top();
            //							push_arr_info(
            //								curr_decl.lower_bound,
            //								curr_decl.upper_bound - curr_decl.lower_bound
            //+
            // 1);
            //							decls.pop();
            //						}
            //						gen(lit, 0,
            // curr_table_item.addr);
            //						gen(iar, 0, depth);
          }
        }
      }
      getSymbol();
      if (sym == semicolon)
        getSymbol();
      else
        error(18);
    }
  }
}

void varDeclaration(vector<pair<int32, int32>> &arrays) {
  if (sym == varsym) {
    getSymbol();
    do
      varDefine(arrays);
    while (sym == ident);
  }
}

typedef pair<string, type> para_pair;
pair<string, type> forParalHelper(bool &isCorrect) {

  if (sym == ident) {
    auto name = last_id;
    getSymbol();
    if (sym == colon) {
      getSymbol();
      type curr_type;
      if (sym & (boolsym | intsym | realsym)) {
        if (sym == boolsym)
          curr_type = boolean;
        else if (sym == intsym)
          curr_type = integer;
        else
          curr_type = real;
        getSymbol();
        isCorrect = true;
        return para_pair(name, curr_type);
      }
      error(error_unexpected_token);
    }
    error(52);
  }
  test(semicolon | rparen, nul, 88);
  isCorrect = false;
  return para_pair("", array);
}

vector<pair<string, type>> forParal() {
  vector<para_pair> parals;
  if (sym == lparen) {
    do {
      getSymbol();
      bool isCorrect;
      auto got = forParalHelper(isCorrect);
      if (isCorrect)
        parals.push_back(got);
      else {
        test(semicolon | rparen, nul, 88);
        return vector<para_pair>();
      }
    } while (sym == semicolon);
    if (sym == rparen)
      getSymbol();
    else
      error(16);
  }
  return parals;
}

void block(uint64, vector<pair<string, type>>, string, funcType);
void funcDefine() {
  if (sym & ((procsym | funcsym) == 0)) {
    error(30);
    return;
  }
  auto is_func = sym == funcsym;
  getSymbol();
  if (sym == ident) {
    auto func_name = last_id;
    funcType func_type;

    getSymbol();
    auto arguments = forParal();
    if (is_func) {
      if (sym == colon) {
        getSymbol();
        if (sym == intsym)
          func_type = integer_ft;
        else if (sym == realsym)
          func_type = real_ft;
        else if (sym == boolsym)
          func_type = boolean_ft;
        else if (sym == arraysym)
          error(error_array_as_argument);
        else
          error(error_unexpected_token);
        getSymbol();
      }
    } else
      func_type = proc_ft;

    if (sym == semicolon) {
      getSymbol();

      vector<type> arg_types;
      for (auto arg : arguments)
        arg_types.push_back(arg.second);

      func_decls.push_back(make_pair(func_type, arg_types));
      last_id = func_name;
      enter(is_func ? function : procedure);

      lev = lev + 1;
      auto tx1 = tx;
      auto dx1 = dx;

      block(decls_sys | statement_sys | period, arguments, func_name,
            func_type);

      lev = lev - 1;
      tx = tx1;
      dx = dx1;

      if (sym == semicolon)
        getSymbol();
      else
        error(33);
    } else
      error(18);
  } else
    error(2);
}

void funcDeclaration() {
  while (sym & (procsym | funcsym))
    funcDefine();
}

void listcode(int32 cx0) { // list code generated for this block
  int32 i;

  for (i = cx0; i <= cx - 1; i++) {
    printf("%10ld%5s%3ld%5ld\n", i, mnemonic[code[i].f], code[i].l, code[i].a);
  }
}

type functype2type(funcType ft) // shouldn't be used when ft could be proc_ft!
{
  switch (ft) {
  case integer_ft:
    return integer;
  case real_ft:
    return real;
  case boolean_ft:
    return boolean;
  default:
    assert(0);
    throw std::exception();
  }
}

void push_real(double d) {
  auto sp = resolve_double(d);
  gen(lit, 0, sp.fst);
  gen(lit, 0, sp.snd);
}

void expression(uint64);
void funcCall(bool isFunc, uint64 /*fsys*/) {
  if (sym == ident) {
    auto fname = last_id;
    auto posi = position(fname);
    if (posi == 0)
      error(error_unknown_ident);
    else {
      auto entry = table[posi];
      if ((entry.kind == function || entry.kind == procedure) &&
          !(isFunc ^ (entry.kind == function))) {
        auto decl = func_decls[entry.val];
        auto func_type = decl.first;
        auto argus = decl.second;
        auto indent = 3;
        auto count_indent = [&](type t) {
          if (t == integer || t == boolean)
            indent += 1;
          if (t == real)
            indent += 2;
          if (t == array)
            assert(0);
        };

        assert(!((entry.kind == procedure) ^ (func_type == proc_ft)));
        getSymbol();
        gen(Int, 0, 3);

        if (sym == lparen) {
          getSymbol();
          if (!argus.empty()) {
            expression(expression_sys);
            if (last_type == integer && argus[0] == real)
              gen(cst, 0, 0);
            else if (last_type != argus[0])
              error(error_type_unmatch);
            count_indent(argus[0]);
            if (argus.size() > 1)
              for (size_t j = 1; j < argus.size(); j += 1) {
                if (sym == comma)
                  getSymbol();
                else
                  error(19);
                expression(expression_sys);
                if (last_type == integer && argus[j] == real)
                  gen(cst, 0, 0);
                else if (last_type != argus[j])
                  error(error_type_unmatch);
                count_indent(argus[j]);
              }
          }
          if (func_type != proc_ft)
            last_type = functype2type(func_type);
          if (sym == rparen)
            getSymbol();
          else
            error(16);
        } else {
          error(error_unexpected_token);
        }
        gen(Int, 0, -indent);
        gen(cal, lev - entry.level, entry.addr);
      } else
        error(error_type_unmatch);
    }
  }
}

void array_access(uint64 fsys) {
  auto sys = fsys;
  if (sym == ident) {
    auto name = last_id;
    auto posi = position(name);
    if (posi == 0)
      error(error_unknown_ident);
    else if (table[posi].kind != array_var)
      error(error_incorrect_type);
    else {
      auto entry = table[posi];
      auto depth = 0;
      auto curr_decl = array_decls[entry.val];
      while (true) {
        depth += 1;
        getSymbol();
        if (sym == lsbra) {
          getSymbol();
          expression(sys);
          if (last_type == integer) {
            if (sym == rsbra)
              getSymbol();
            else
              error(51);
          } else
            error(error_type_unmatch);
        } else
          error(50);
        if (curr_decl.item_type == array)
          curr_decl = array_decls[curr_decl.item_id];
        else
          break;
      }
      auto bottom_type = curr_decl.item_type;
      if (bottom_type == array)
        assert(0);
      last_type = bottom_type;
      gen(lit, 0, depth);
      push_type(bottom_type);
      gen(arr, lev - entry.level, entry.addr);
    }
  } else
    error(error_incorrect_type);
}

void array_init(int32 arr_idx, int32 decl_idx) {
  auto type_define_item = table[decl_idx];
  auto &curr_table_item = table[arr_idx];
  auto decl_num = type_define_item.addr;
  curr_table_item.val = decl_num;
  auto push_arr_info = [=](int start, int width) {
    gen(lit, 0, start);
    gen(lit, 0, width);
  };

  if (type_define_item.kind != array_define ||
      curr_table_item.kind != array_var)
    assert(0);

  stack<array_decl> decls;
  type bottom_type;

  while (true) {
    auto curr_decl = array_decls[decl_num];
    decls.push(curr_decl);
    if (curr_decl.item_type == array)
      decl_num = curr_decl.item_id;
    else {
      bottom_type = curr_decl.item_type;
      break;
    }
  }
  push_type(bottom_type);
  auto depth = decls.size();
  while (!decls.empty()) {
    auto curr_decl = decls.top();
    push_arr_info(curr_decl.lower_bound,
                  curr_decl.upper_bound - curr_decl.lower_bound + 1);
    decls.pop();
  }
  gen(lit, 0, curr_table_item.addr);
  gen(iar, 0, depth);
}

type get_bottom_type(int array_decls_index) {
  if (array_decls_index < 0 ||
      array_decls_index >= static_cast<int>(array_decls.size()))
    assert(0);
  auto curr_decl = array_decls[array_decls_index];
  while (curr_decl.item_type == array)
    curr_decl = array_decls[curr_decl.item_id];
  assert(curr_decl.item_type != array);
  return curr_decl.item_type;
}

void identRef_in_factor(uint64 fsys) {
  auto sys = fsys;
  auto i = position(last_id);
  if (i == 0) {
    error(error_unknown_ident);
    getSymbol();
  } else {
    auto item_entry = table[i];
    switch (item_entry.kind) {
    case int_const:
      gen(lit, 0, item_entry.val);
      last_type = integer;
      break;
    case real_const: {
      auto val = reals[item_entry.addr];
      push_real(val);
      last_type = real;
      break;
    }
    case int_var:
      pyp_int();
      gen(lod, lev - item_entry.level, item_entry.addr);
      last_type = integer;
      break;
    case real_var:
      pyp_real();
      gen(lod, lev - item_entry.level, item_entry.addr);
      last_type = real;
      break;
    case bool_var:
      pyp_bool();
      gen(lod, lev - item_entry.level, item_entry.addr);
      last_type = boolean;
      break;
    case array_var: // get an item in array
    {
      array_access(sys);
      push_type(get_bottom_type(item_entry.val));
      gen(rhp, 0, 0);
      break;
    }
    case function:
      funcCall(true, sys);
      break;
    default:
      error(error_incorrect_type);
      getSymbol();
      return;
    }
    if (item_entry.kind != array_var && item_entry.kind != function)
      getSymbol();
  }
}

void simpExpr(uint64);
void factor(uint64 fsys) {
  auto sys = fsys;
  //	test(factor_sys, fsys, 24);
  while (sym & factor_sys) {
    if (sym == ident)
      identRef_in_factor(sys);
    else if (sym == number) {
      if (last_type == integer) {
        gen(lit, 0, last_int);
        last_type = integer;
      } else if (last_type == real) {
        last_type = real;
        dni dp;
        dp.d = last_real;
        gen(lit, 0, dp.fst);
        gen(lit, 0, dp.snd);
      } else
        error(error_unexpected_token);
      getSymbol();
    } else if (sym == lparen) {
      getSymbol();
      expression(rparen | fsys);
      if (sym == rparen)
        getSymbol();
      else
        error(22);
    } else if (sym == notsym) {
      getSymbol();
      factor(fsys);
      if (last_type == boolean)
        gen(opr, 0, 14);
      last_type = boolean;
    } else if (sym == truesym || sym == falsesym) {
      gen(lit, 0, sym == truesym ? 1 : 0);
      last_type = boolean;
      getSymbol();
    } else if (sym == oddsym) {
      getSymbol();
      if (sym == lparen) {
        getSymbol();
        simpExpr(sys);
        if (last_type == integer)
          gen(opr, 0, 6);
        last_type = boolean;
        if (sym == rparen)
          getSymbol();
        else
          error(16);
      } else
        error(15);
    }
    //		test(fsys, lparen, 23);
  }
  assert(last_type != array);
}

void term(uint64 fsys) {
  uint64 term_op;
  uint64 sys = fsys | times | slash | divsym | modsym | andsym;
  factor(sys);
  auto base_type = last_type;
  //	if (last_int == 4) assert(0);

  while (sym & (times | slash | divsym | modsym | andsym)) {
    term_op = sym;
    getSymbol();
    factor(sys);
    auto curr_type = last_type;
    assert(curr_type != array);
    if (term_op == andsym) {
      if (base_type == boolean && curr_type == boolean) {
        pyp_bool();
        gen(opr, 0, 15);
      } else
        error(error_type_unmatch);
      last_type = boolean;
    } else if (term_op & (divsym | modsym)) {
      if (base_type == integer && curr_type == integer) {
        pyp_int();
        gen(opr, 0, term_op == divsym ? op_div : op_mod);
      } else
        error(error_type_unmatch);
      last_type = integer;
    } else {
      if (base_type == integer && curr_type == integer) {
        last_type = term_op == times ? integer : real;
        pyp_int();
        gen(opr, 0, term_op == times ? op_times : op_slash);

      } else if (base_type == real && curr_type == real) {
        last_type = real;
        pyp_real();
        gen(opr, 0, term_op == times ? op_times : op_slash);
      } else if (base_type == real && curr_type == integer) {
        last_type = real;
        gen(cst, 0, 0);
        pyp_real();
        gen(opr, 0, term_op == times ? op_times : op_slash);
      } else if (base_type == integer && curr_type == real) {
        last_type = real;
        gen(pop, 0, real);
        gen(cst, 0, 0);
        gen(psh, 0, real);
        pyp_real();
        gen(opr, 0, term_op == times ? op_times : op_slash);
      } else {
        error(error_type_unmatch);
      }
      //			getSymbol();
    }
  }
}

void simpExpr(uint64 fsys) {
  auto has_head = false;
  auto head_sym = plus;
  auto sys = fsys | plus | minus | orsym;
  if (sym & (plus | minus)) {
    head_sym = sym;
    has_head = true;
    getSymbol();
  }
  term(sys);

  auto base_type = last_type;
  if (has_head && base_type == boolean)
    error(error_type_unmatch);
  else if (base_type == boolean) {
    while (sym == orsym) {
      getSymbol();
      term(sys);
      if (last_type == boolean) {
        pyp_bool();
        gen(opr, 0, op_or);
      } else {
        error(error_type_unmatch);
        getSymbol();
      }
    }
  } else if (last_type == array) {
    printf("NO ARRAY AS A TYPE\n");
    error(error_needs_interupt);

  } else // last type is integer or real
  {
    if (has_head && head_sym == minus) {
      push_type(last_type == integer ? integer : real);
      gen(opr, 0, 1);
    }
    while (sym & (plus | minus)) {
      auto last_op = sym;
      getSymbol();
      term(sys);
      auto curr_type = last_type;
      if (base_type == integer && curr_type == integer) {
        pyp_int();
        gen(opr, 0, last_op == plus ? op_plus : op_minus);
      } else if (base_type == real && curr_type == real) {
        pyp_real();
        gen(opr, 0, last_op == plus ? op_plus : op_minus);
      } else if (base_type == real && curr_type == integer) {
        last_type = real;
        gen(cst, 0, 0);
        pyp_real();
        gen(opr, 0, last_op == plus ? op_plus : op_minus);
      } else if (base_type == integer && curr_type == real) {
        gen(pop, 0, real);
        gen(cst, 0, 0);
        gen(psh, 0, real);
        pyp_real();
        gen(opr, 0, last_op == plus ? op_plus : op_minus);
      } else
        error(error_type_unmatch);
      //			getSymbol();
    }
  }
}

void expression(uint64 fsys) {
  auto sys = fsys;
  simpExpr(sys | eql | neq | lss | gtr | leq | geq);
  auto base_type = last_type;

  if (sym & (eql | neq | lss | gtr | leq | geq)) {
    auto last_op = sym;
    getSymbol();
    simpExpr(fsys);
    auto curr_type = last_type;

    if (base_type == boolean && curr_type == boolean) {
      if (last_op & (eql | neq)) {
        pyp_bool();
        gen(opr, 0, last_op == eql ? op_eq : op_neq);
      } else {
        error(error_unexpected_token);
        getSymbol();
      }
    } else if ((base_type == integer || base_type == real) &&
               (curr_type == integer || curr_type == real)) {
      if (base_type == integer && curr_type == integer) {
        pyp_int();
      } else if (base_type == real && curr_type == real) {
        pyp_real();
      } else if (base_type == real && curr_type == integer) {
        last_type = real;
        gen(cst, 0, 0);
        pyp_real();
      } else {
        gen(pop, 0, real);
        gen(cst, 0, 0);
        gen(psh, 0, real);
        pyp_real();
      }

      if (last_op & (eql | neq))
        gen(opr, 0, last_op == eql ? op_eq : op_neq);
      else if (last_op & (lss | leq))
        gen(opr, 0, last_op == lss ? op_lt : op_le);
      else if (last_op & (gtr | geq))
        gen(opr, 0, last_op == gtr ? op_gt : op_ge);
      else
        assert(0);
    }
    last_type = boolean;
  }
}

void statement(uint64 fsys) {
  auto sys = fsys;
  static stack<vector<int>> while_stack;
  if (sym == ident) {
    auto name = last_id;
    auto posi = position(name);
    auto escape = []() {
      test(semicolon, semicolon | endsym, error_unexpected_token);
    };
    if (posi == 0)
      error(error_unknown_ident);
    else {
      auto entry = table[posi];
      switch (entry.kind) {
      case int_const:
        error(error_const_assign);
        escape();
        break;
      case real_const:
        error(error_const_assign);
        escape();
        break;
      case procedure:
      case function:
      case array_define:
        error(error_type_unmatch);
        escape();
        break;
      case int_var:
      case real_var:
      case array_var:
      case bool_var:
        break;
      }

      auto kind = entry.kind;
      if (kind == int_var || kind == real_var || kind == bool_var) {
        getSymbol();
        if (sym & (becomes | eql)) {
          if (sym == eql)
            error(35);
          getSymbol();
          expression(fsys | endsym | semicolon);
          auto rtype = last_type;
          if ((kind == bool_var && rtype == boolean) ||
              (kind == int_var && rtype == integer) ||
              (kind == real_var && rtype == real)) {
            push_type(rtype);
            gen(sto, lev - entry.level, entry.addr);
          } else if (kind == real_var && rtype == integer) {
            gen(cst, 0, 0);
            pyp_real();
            gen(sto, lev - entry.level, entry.addr);
          } else
            error(error_type_unmatch);
        } else
          error(35);
      } else if (kind == array_var) {
        array_access(sys);

        if (sym & (becomes | eql)) {
          if (sym == eql)
            error(35);
          getSymbol();
          expression(fsys | endsym | semicolon);
          auto rtype = last_type;
          auto atype = array_decls[entry.val];
          while (atype.item_type == array)
            atype = array_decls[atype.item_id];
          auto bottom_type = atype.item_type;
          if (rtype == bottom_type) {
            push_type(rtype);
            gen(whp, 0, 0);
          } else if (rtype == integer && bottom_type == real) {
            gen(cst, 0, 0);
            pyp_real();
            gen(whp, 0, 0);
          }
        } else
          error(35);
      }
    }
  } else if (sym == ifsym) {
    getSymbol();
    expression(sys);
    if (last_type == boolean) {
      gen(jpc, 0, 0);
      auto cx1 = cx;
      if (sym == thensym) {
        getSymbol();
        statement(sys);
        gen(jmp, 0, 0);
        code[cx1 - 1].a = cx;
        auto cx2 = cx;
        if (sym == elsesym) {
          getSymbol();
          statement(sys);
        }
        code[cx2 - 1].a = cx;
      } else
        error(24);
    } else {
      error(error_incorrect_type);
    }
  } else if (sym == beginsym) {

    do {
      getSymbol();
      statement(sys);
    } while (sym != endsym);
    getSymbol();
  } else if (sym == writesym) {
    getSymbol();
    if (sym == lparen) {
      do {
        getSymbol();
        expression(sys | comma | rparen);
        gen(opt, 0, last_type);
      } while (sym == comma);
      if (sym == rparen)
        getSymbol();
      else
        error(31);
    } else
      error(30);
  } else if (sym == whilesym) {
    auto cx1 = cx;
    getSymbol();
    expression(sys);
    auto cx2 = cx;
    gen(jpc, 0, 0);
    while_stack.push(vector<int>());
    if (last_type != boolean)
      error(error_type_unmatch);
    //		getSymbol();
    if (sym == dosym) {
      getSymbol();
      statement(sys);
      gen(jmp, 0, cx1);
      code[cx2].a = cx;
    } else
      error(26);
    auto tails = while_stack.top();
    for (auto tail : tails)
      code[tail].a = cx;
    while_stack.pop();
  } else if (sym == exitsym) {
    gen(jmp, 0, 0);
    while_stack.top().push_back(cx - 1);
  } else if (sym == callsym) {
    getSymbol();
    funcCall(false, sys);
  } else if (sym == readsym) {
    getSymbol();
    if (sym == lparen) {
      getSymbol();
      do {
        if (sym == ident) {
          auto name = last_id;
          auto posi = position(name);
          auto escape = []() {
            test(semicolon, semicolon | endsym, error_unexpected_token);
          };
          if (posi == 0)
            error(error_unknown_ident);
          else {
            auto entry = table[posi];
            switch (entry.kind) {
            case int_const:
              error(error_const_assign);
              escape();
              break;
            case real_const:
              error(error_const_assign);
              escape();
              break;
            case procedure:
            case function:
            case array_define:
              error(error_type_unmatch);
              escape();
              break;
            case int_var:
            case real_var:
            case array_var:
            case bool_var:
              break;
            }

            auto kind = entry.kind;
            if (kind == int_var || kind == real_var || kind == bool_var) {
              type read_type;
              if (kind == int_var)
                read_type = integer;
              else if (kind == real_var)
                read_type = real;
              else
                read_type = boolean;
              gen(ipt, 0, read_type);
              push_type(read_type);
              gen(sto, lev - entry.level, entry.addr);
            } else if (kind == array_var) {
              array_access(sys);
              auto bottom_type = get_bottom_type(entry.val);
              gen(ipt, 0, bottom_type);
              push_type(bottom_type);
              gen(whp, 0, 0);
            }
          }
        }
      } while (sym == comma);
    }
  }
}

void block(uint64 fsys, vector<pair<string, type>> arguments, string funcName,
           funcType return_type) {
  int32 tx0; // initial table index

  dx = 3;
  tx0 = tx;
  auto &calling = table[tx0];
  calling.addr = cx;
  gen(jmp, 0, 0);
  auto cx2 = cx - 1;

  if (cx > 130)
    assert(0);

  if (lev > levmax)
    error(32);
  for (auto arg : arguments) {
    last_id = arg.first;
    auto arg_t = arg.second;
    if (arg_t == integer)
      enter(int_var);
    else if (arg_t == real)
      enter(real_var);
    else
      enter(bool_var);
  }

  last_id = funcName;
  switch (return_type) // adding the return value
  {
  case real_ft:
    enter(real_var);
    break;
  case boolean_ft:
    enter(bool_var);
    break;
  case integer_ft:
    enter(int_var);
    break;
  case proc_ft:
    break;
  }
  auto return_addr = table[tx].addr;
  vector<pair<int32, int32>> array_needing_init;

  constDeclaration();
  typeDeclaration();
  varDeclaration(array_needing_init);
  funcDeclaration();

  test(statement_sys | ident, decls_sys, 7);
  code[cx2].a = cx;
  //	table[tx0].addr = cx; // start addr of code
  gen(Int, 0, dx);

  for (auto ad : array_needing_init)
    array_init(ad.first, ad.second);

  statement(fsys | semicolon | endsym);

  switch (return_type) {
  case integer_ft:
  case boolean_ft:
    gen(lit, 0, return_addr);
    if (return_type == integer_ft)
      pyp_int();
    else
      pyp_bool();
    break;
  case real_ft:
    gen(lit, 0, return_addr);
    pyp_real();
    break;
  case proc_ft:
    gen(pyp, 0, proc_return_specifier);
    break;
  }

  gen(opr, 0, 0); // return
  //	test(fsys, 0, 8);
  //	listcode(cx0);
}

int32 base(int32 b, int32 l) {
  int32 b1;

  b1 = b;
  while (l > 0) { // find base l levels down
    b1 = s[b1];
    l = l - 1;
  }
  return b1;
}

void interpret() {
  int32 p, b, t; // program-, base-, topstack-registers
  instruction i; // instruction register
  int32 breakpoint = -1;

  auto is_ipt_boolean = [=](int32 bi) {
    return bi == ipt_true || bi == ipt_false;
  };
  auto get_stack_addr = [&](int32 offset) { return &s[offset]; };
  auto halt = [&]() { code[p].f = hlt; };

  int32 reg_i;
  double reg_f;
  string halting_message;
  auto haltwith = [&](string msg) {
    halting_message = msg;
    halt();
  };

  printf("start PL/0\n");
  t = 0;
  b = 1;
  p = 0;
  s[1] = 0;
  s[2] = 0;
  s[3] = 0;
  do {
    if (p == breakpoint)
      assert(0);
    i = code[p]; // load instruction
    p = p + 1;   // move to next instruction sn

    //		printf("counter = %3d,, p = %3d, i.f = %2d, i.a = %5d\n", counter,
    //p
    //- 1, i.f, i.a);
    //		counter += 1;

    switch (i.f) {
    // case 0:
    //   printf("Abnormal exit\n");
    //   p = 0;
    //   break;
    case lit: // push int_const
      t = t + 1;
      s[t] = i.a;
      break;
    case opr:
      switch (i.a) { // operator
      case op_ret:   // return
      {
        auto b0 = b;
        p = s[b0 + 2];
        b = s[b0 + 1];
        auto r_type = s[t];
        auto r_addr = s[t - 1];
        if (r_type == integer || r_type == boolean || r_type == real) {
          t = b0;
          s[b0] = s[b0 + r_addr];
          if (r_type == real) {
            t = b0 + 1;
            s[b0 + 1] = s[b0 + r_addr + 1];
          }
        } else if (r_type == proc_return_specifier)
          t = b0 - 1;
        else
          assert(0);

        break;
      }
      case op_neg: // neg
      {
        if (s[t] == integer) {
          s[t - 1] = -s[t - 1];
          t = t - 1;
        } else if (s[t] == real) {
          dni dp;
          dp.fst = s[t - 2];
          dp.snd = s[t - 1];
          auto dm = resolve_double(-dp.d);
          s[t - 2] = dm.fst;
          s[t - 1] = dm.snd;
          t = t - 1;
        } else
          assert(0);
        break;
      }
      case op_plus: // plus
      {
        if (s[t] == integer) {
          s[t - 2] = s[t - 2] + s[t - 1];
          t = t - 2;
        } else if (s[t] == real) {
          auto d1 = regain_double(s[t - 2], s[t - 1]);
          auto d2 = regain_double(s[t - 4], s[t - 3]);
          auto sum = d1 + d2;
          auto dp = resolve_double(sum);
          s[t - 4] = dp.fst;
          s[t - 3] = dp.snd;
          t = t - 3;
        } else
          assert(0);
        break;
      }
      case op_minus: // minus
      {
        if (s[t] == integer) {
          s[t - 2] = s[t - 2] - s[t - 1];
          t = t - 2;
        } else if (s[t] == real) {
          auto d1 = regain_double2(&s[t - 2]);
          auto d2 = regain_double2(&s[t - 4]);
          auto diff = d2 - d1;
          writeDouble(diff, &s[t - 4]);
          t = t - 3;
        } else
          assert(0);
        break;
      }
      case op_times: // times
      {
        if (s[t] == integer) {
          s[t - 2] = s[t - 2] * s[t - 1];
          t = t - 2;
        } else if (s[t] == real) {
          auto d1 = regain_double2(&s[t - 2]);
          auto d2 = regain_double2(&s[t - 4]);
          auto product = d2 * d1;
          writeDouble(product, &s[t - 4]);
          t = t - 3;
        } else
          assert(0);
        break;
      }
      case op_slash: // float div
      {
        if (s[t] == integer) {
          auto d = double(s[t - 2]) / double(s[t - 1]);
          writeDouble(d, &s[t - 2]);
          t = t - 1;
        } else if (s[t] == real) {
          auto d1 = regain_double2(&s[t - 2]);
          auto d2 = regain_double2(&s[t - 4]);
          auto quotient = d2 / d1;
          writeDouble(quotient, &s[t - 4]);
          t = t - 3;
        } else
          assert(0);
        break;
      }
      case op_odd: // isOdd
        s[t] = s[t] % 2 == 1 ? ipt_true : ipt_false;
        break;
      case op_eq: // eq
      {
        if (s[t] == integer) {
          s[t - 2] = s[t - 2] == s[t - 1] ? ipt_true : ipt_false;
          t = t - 2;
        } else if (s[t] == real) {
          auto d1 = regain_double2(&s[t - 2]);
          auto d2 = regain_double2(&s[t - 4]);
          s[t - 4] = d1 == d2 ? ipt_true : ipt_false;
          t = t - 4;
        } else if (s[t] == boolean) {
          if (is_ipt_boolean(s[t - 1]) && is_ipt_boolean(s[t - 2])) {
            s[t - 2] = s[t - 2] == s[t - 1] ? ipt_true : ipt_false;
            t = t - 2;
          } else
            assert(0);
        } else
          assert(0);
        break;
      }
      case op_neq: // neq
      {
        if (s[t] == integer) {
          s[t - 2] = s[t - 2] != s[t - 1] ? ipt_true : ipt_false;
          t = t - 2;
        } else if (s[t] == real) {
          auto d1 = regain_double2(&s[t - 2]);
          auto d2 = regain_double2(&s[t - 4]);
          s[t - 4] = d1 != d2 ? ipt_true : ipt_false;
          t = t - 4;
        } else if (s[t] == boolean) {
          if (is_ipt_boolean(s[t - 1]) && is_ipt_boolean(s[t - 2])) {
            s[t - 2] = s[t - 2] != s[t - 1] ? ipt_true : ipt_false;
            t = t - 2;
          } else
            assert(0);
        } else
          assert(0);
        break;
      }
      case op_lt: // lt
      {
        if (s[t] == integer) {
          s[t - 2] = s[t - 2] < s[t - 1] ? ipt_true : ipt_false;
          t = t - 2;
        } else if (s[t] == real) {
          auto d1 = regain_double2(&s[t - 2]);
          auto d2 = regain_double2(&s[t - 4]);
          s[t - 4] = d2 < d1 ? ipt_true : ipt_false;
          t = t - 4;
        } else
          assert(0);
        break;
      }
      case op_ge: // ge
      {
        if (s[t] == integer) {
          s[t - 2] = s[t - 2] >= s[t - 1] ? ipt_true : ipt_false;
          t = t - 2;
        } else if (s[t] == real) {
          auto d1 = regain_double2(&s[t - 2]);
          auto d2 = regain_double2(&s[t - 4]);
          s[t - 4] = d2 >= d1 ? ipt_true : ipt_false;
          t = t - 4;
        } else
          assert(0);
        break;
      }
      case op_gt: // gt
      {
        if (s[t] == integer) {
          s[t - 2] = s[t - 2] > s[t - 1] ? ipt_true : ipt_false;
          t = t - 2;
        } else if (s[t] == real) {
          auto d1 = regain_double2(&s[t - 2]);
          auto d2 = regain_double2(&s[t - 4]);
          s[t - 4] = d2 > d1 ? ipt_true : ipt_false;
          t = t - 4;
        } else
          assert(0);
        break;
      }
      case op_le: // le
      {
        if (s[t] == integer) {
          s[t - 2] = s[t - 2] <= s[t - 1] ? ipt_true : ipt_false;
          t = t - 2;
        } else if (s[t] == real) {
          auto d1 = regain_double2(&s[t - 2]);
          auto d2 = regain_double2(&s[t - 4]);
          s[t - 4] = d2 <= d1 ? ipt_true : ipt_false;
          t = t - 4;
        } else
          assert(0);
        break;
      }
      case op_not: // not
      {
        if (s[t] == boolean && is_ipt_boolean(s[t - 1])) {
          s[t - 1] = s[t - 1] == ipt_true ? ipt_false : ipt_true;
          t = t - 1;
        } else
          assert(0);
        break;
      }
      case op_and: // and
      {
        if (s[t] == boolean && is_ipt_boolean(s[t - 1]) &&
            is_ipt_boolean(s[t - 2])) {
          s[t - 2] = s[t - 2] == ipt_true
                         ? (s[t - 1] == ipt_true ? ipt_true : ipt_false)
                         : ipt_false;
          t = t - 2;
        } else
          assert(0);
        break;
      }
      case op_or: // or
      {
        if (s[t] == boolean && is_ipt_boolean(s[t - 1]) &&
            is_ipt_boolean(s[t - 2])) {
          s[t - 2] = s[t - 2] == ipt_false
                         ? (s[t - 1] == ipt_true ? ipt_true : ipt_false)
                         : ipt_true;
          t = t - 2;
        } else
          assert(0);
        break;
      }
      case op_div: // integer div
      {
        if (s[t] != integer)
          assert(0);
        auto i1 = s[t - 1];
        auto i2 = s[t - 2];
        s[t - 2] = i2 / i1;
        t = t - 2;
        break;
      }
      case op_mod: // mod
      {
        auto i1 = s[t];
        auto i2 = s[t - 1];
        s[t - 1] = i2 % i1;
        t = t - 1;
        break;
      }
      default:
        assert(0);
        break;
      }
      break;
    case lod: {
      if (s[t] == integer || s[t] == boolean) {
        auto addr = base(b, i.l) + i.a;
        if (s[t] == boolean && !is_ipt_boolean(s[addr]))
          assert(0);
        s[t] = s[addr];
      } else if (s[t] == real) {
        auto addr = base(b, i.l) + i.a;
        s[t] = s[addr];
        s[t + 1] = s[addr + 1];
        t = t + 1;
      } else
        assert(0);
      break;
    }
    case sto: {
      if (s[t] == integer || s[t] == boolean) {
        auto addr = base(b, i.l) + i.a;
        if (s[t] == boolean && !is_ipt_boolean(s[t - 1]))
          assert(0);
        s[addr] = s[t - 1];
        t = t - 2;
      } else if (s[t] == real) {
        auto addr = base(b, i.l) + i.a;
        s[addr] = s[t - 2];
        s[addr + 1] = s[t - 1];
        t = t - 3;
      } else
        assert(0);
      break;
    }
    case cal: // generate new block mark
      s[t + 1] = base(b, i.l);
      s[t + 2] = b; //
      s[t + 3] = p; // return address
      b = t + 1;
      p = i.a;
      break;
    case Int:
      t = t + i.a;
      break;
    case jmp:
      p = i.a;
      break;
    case jpc:
      if (!is_ipt_boolean(s[t]))
        assert(0);
      if (s[t] == ipt_false)
        p = i.a;
      t = t - 1;
      break;
    case cst:
      writeDouble(double(s[t]), &s[t]);
      t = t + 1;
      break;
    case pyp: {
      t = t + 1;
      s[t] = i.a;
      break;
    }
    case pop: {
      if (i.a == integer || i.a == boolean) {
        if (i.a == boolean && !is_ipt_boolean(s[t]))
          assert(0);
        reg_i = s[t];
        t = t - 1;
      } else if (i.a == real) {
        reg_f = regain_double(s[t - 1], s[t]);
        t = t - 2;
      } else
        assert(0);
      break;
    }
    case psh: {
      if (i.a == integer || i.a == boolean) {
        if (i.a == boolean && !is_ipt_boolean(reg_i))
          assert(0);
        s[t + 1] = reg_i;
        t = t + 1;
      } else if (i.a == real) {
        // ReSharper disable once CppLocalVariableMightNotBeInitialized
        writeDouble(reg_f, &s[t + 1]);
        t = t + 2;
      } else
        assert(0);
      break;
    }
    case opt: {
      if (i.a == integer) {
        printf("%ld\n", s[t]);
        t = t - 1;
      } else if (i.a == boolean) {
        printf("%s\n", s[t] == ipt_true ? "true" : "false");
        t = t - 1;
      } else if (i.a == real) {
        auto d = regain_double2(get_stack_addr(t - 1));
        printf("%lf\n", d);
        t = t - 2;
      } else
        assert(0);
      break;
    }
    case ipt: {
      auto tid = i.a;
      if (tid == integer) {
        int ia;
        scanf("%d", &ia);
        s[t + 1] = ia;
        t = t + 1;
      } else if (tid == real) {
        double di;
        scanf("%lf", &di);
        writeDouble(di, get_stack_addr(t + 1));
        t = t + 2;
      } else if (tid == boolean) {
        const auto buffer_size = 20;
        char buffer[buffer_size];
        fgets(buffer, buffer_size, stdin);
        if (strcmp(buffer, "true") == 0) {
          s[t + 1] = ipt_true;
          t = t + 1;
        } else if (strcmp(buffer, "false") == 0) {
          s[t + 1] = ipt_false;
          t = t + 1;
        } else
          haltwith("Not Valid Boolean Literial!");
      } else
        assert(0);
      break;
    }
    case rhp: {
      if (s[t] == integer || s[t] == boolean) {
        auto addr = s[t - 1];
        auto got = *reinterpret_cast<int32 *>(addr);
        if (s[t] == boolean && !is_ipt_boolean(got))
          assert(0);
        s[t - 1] = got;
        t = t - 1;
      } else if (s[t] == real) {
        auto addr = s[t - 1];
        auto got = *reinterpret_cast<double *>(addr);
        writeDouble(got, get_stack_addr(t - 1));
      } else
        assert(0);
      break;
    }
    case whp: {
      if (s[t] == integer || s[t] == boolean) {
        auto addr = reinterpret_cast<int32 *>(s[t - 2]);
        if (s[t] == boolean && !is_ipt_boolean(s[t - 1]))
          assert(0);
        addr[0] = s[t - 1];
        t = t - 3;
      } else if (s[t] == real) {
        auto addr = reinterpret_cast<double *>(s[t - 3]);
        addr[0] = regain_double(s[t - 2], s[t - 1]);
        t = t - 4;
      } else
        assert(0);
      break;
    }
    case iar: {
      auto depth = i.a;
      auto target_addr = s[t];
      auto nt = t - 2;
      auto root = new yyArray();
      auto size = 1;
      for (auto j = 0; j < depth; j += 1) {
        auto begin = s[nt];
        auto width = s[nt + 1];
        if (width <= 0)
          assert(0);
        size *= width;
        root->bounds.push_back(yyArray::ArrayBound(begin, width));
        nt = nt - 2;
      }
      auto tid = s[nt + 1];
      if (tid == integer || tid == boolean) {
      } else if (tid == real)
        size *= 2;
      else
        assert(0);
      root->items = malloc(size);
      root->array_type = type(tid);
      s[base(b, 0) + target_addr] = int32(root);
      t = nt;
      break;
    }
    case arr: {
      auto tid = s[t];
      unsigned depth = s[t - 1];
      auto nt = t - 2;
      auto arr_posi = s[base(b, i.l) + i.a];
      auto array_root = reinterpret_cast<yyArray *>(arr_posi);
      stack<int> idxs;

      if (array_root->bounds.size() != depth)
        assert(0);
      auto index = 0;
      auto last_width = 0;

      for (size_t j = 0; j < depth; j += 1, nt = nt - 1) {
        auto curr_idx = s[nt];
        auto bound = array_root->bounds[depth - 1 - j];
        auto offset = curr_idx - bound.begin;
        if (offset < 0 || offset >= bound.width)
          haltwith("Out of Range!");
        index = index * last_width + offset;
        last_width = bound.width;
      }
      t = nt + 1;
      void *addr = nullptr;
      if (tid == integer || tid == boolean)
        addr = reinterpret_cast<void *>(
            &static_cast<int32 *>(array_root->items)[index]);
      else if (tid == real)
        addr = static_cast<void *>(
            &static_cast<double *>(array_root->items)[index]);
      else
        assert(0);

      s[t] = int32(addr);
      break;
    }
    case hlt: {
      printf("%s\n", halting_message.c_str());
      getchar();
      exit(1);
    }
    }
  } while (p != 0);
  printf("end PL/0\n");
  //	assert(0);
}

void init_front_end() {
  for (size_t i = 0; i < 256; i++)
    ssym[i] = nul;

  ssym[static_cast<size_t>('+')] = plus;
  ssym[static_cast<size_t>('-')] = minus;
  ssym[static_cast<size_t>('*')] = times;
  ssym[static_cast<size_t>('/')] = slash;
  ssym[static_cast<size_t>('(')] = lparen;
  ssym[static_cast<size_t>(')')] = rparen;
  ssym[static_cast<size_t>('=')] = eql;
  ssym[static_cast<size_t>(',')] = comma;
  ssym[static_cast<size_t>('.')] = period;
  ssym[static_cast<size_t>(';')] = semicolon;
  ssym[static_cast<size_t>('[')] = lsbra;
  ssym[static_cast<size_t>(']')] = rsbra;

  typedef pair<string, sym_t> sym_pair;

  reservedWords.insert(sym_pair("and", andsym));
  reservedWords.insert(sym_pair("array", arraysym));
  reservedWords.insert(sym_pair("begin", beginsym));
  reservedWords.insert(sym_pair("Boolean", boolsym));
  reservedWords.insert(sym_pair("call", callsym));
  reservedWords.insert(sym_pair("const", constsym));
  reservedWords.insert(sym_pair("div", divsym));
  reservedWords.insert(sym_pair("do", dosym));
  reservedWords.insert(sym_pair("else", elsesym));
  reservedWords.insert(sym_pair("end", endsym));
  reservedWords.insert(sym_pair("exit", exitsym));
  reservedWords.insert(sym_pair("false", falsesym));
  reservedWords.insert(sym_pair("function", funcsym));
  reservedWords.insert(sym_pair("if", ifsym));
  reservedWords.insert(sym_pair("integer", intsym));
  reservedWords.insert(sym_pair("mod", modsym));
  reservedWords.insert(sym_pair("not", notsym));
  reservedWords.insert(sym_pair("odd", oddsym));
  reservedWords.insert(sym_pair("of", ofsym));
  reservedWords.insert(sym_pair("or", orsym));
  reservedWords.insert(sym_pair("procedure", procsym));
  reservedWords.insert(sym_pair("read", readsym));
  reservedWords.insert(sym_pair("real", realsym));
  reservedWords.insert(sym_pair("then", thensym));
  reservedWords.insert(sym_pair("true", truesym));
  reservedWords.insert(sym_pair("type", typesym));
  reservedWords.insert(sym_pair("var", varsym));
  reservedWords.insert(sym_pair("while", whilesym));
  reservedWords.insert(sym_pair("write", writesym));

  strcpy(mnemonic[lit], "lit");
  strcpy(mnemonic[opr], "opr");
  strcpy(mnemonic[lod], "lod");
  strcpy(mnemonic[sto], "sto");
  strcpy(mnemonic[cal], "cal");
  strcpy(mnemonic[Int], "int");
  strcpy(mnemonic[jmp], "jmp");
  strcpy(mnemonic[jpc], "jpc");
  strcpy(mnemonic[cst], "cst");
  //	decls_sys = constsym | varsym | procsym;
  //	statement_sys = beginsym | callsym | ifsym | whilesym;
  //	factor_sys = ident | number | lparen;

  err = 0;
  charCount = 0;
  cx = 0;
  lineLength = 0;
  ch = ' ';
  getSymbol();
  lev = 0;
  tx = 0;
}

// int main(int argc, char** argv)
//{
//	if (argc != 2) {
//		printf("but command\n");
//		exit(-1);
//	}
//
//	strcpy(infilename, strlen(argv[1]), argv[1]);
//	printf("The name of source file is %s\n", infilename);
//
//	if ((infile = fopen(infilename, "r")) == nullptr)
//	{
//		printf("File %s can't be opened.\n", infilename);
//		exit(1);
//	}
//
//	init_front_end();
//
//
//	block(decls_sys | statement_sys | period);
//	if (sym != period) {
//		error(9);
//	}
//	if (err == 0) {
//		interpret();
//	}
//	else {
//		printf("errors in PL/0 program\n");
//	}
//	fclose(infile);
//	return 0;
//}

void test1() {
  if (sym == number && last_type == integer) {
    auto number1 = last_int;
    getSymbol();
    if (sym == tillsym) {
      getSymbol();
      if (sym == number && last_type == integer) {
        auto number2 = last_int;
        printf("succeed\n");
        printf("%ld .. %ld\n", number1, number2);
      } else
        error(3);
    } else
      error(2);
  } else
    error(1);
}

void test2() { typeDeclaration(); }

void test3() {
  if (sym == number && last_type == real)
    printf("succeed: %lf\n", last_real);
}

void test4() { constDeclaration(); }

// void test5()
//{
//	typeDeclaration();
//	varDeclaration();
//}

void test6() {
  expression(expression_sys | period);
  if (sym == period) {
  } else
    error(19);
  interpret();
}

void test7() {
  statement(statement_sys);
  if (sym == period) {
  } else
    error(19);
  interpret();
}

void test8() {
  vector<pair<string, type>> a;
  block(block_sym, a, "main", proc_ft);
  if (sym == period) {
  } else
    error(19);

  if (err == 0)
    interpret();
}

auto source0 = "test.pl0";
auto source1 = "C:\\temp_files\\yyctest\\typedefine.yy";
auto source2 = "C:\\temp_files\\yyctest\\number.yy";
auto source3 = "constdefine.yy";
auto source5 = "vardefine.yy";
auto source6 = "exp1.yy";
auto source7 = "statement1.yy";
auto source8 = "block1.yy";
auto source9 = "block2.yy";
auto source10 = "block3.yy";
auto source11 = "PreTest10.pl0";
auto source12 = "C:\\Users\\Stephen\\Desktop\\tmp\\RealFactorial40.pl0";

void testFunc() {
  strncpy(infilename, source0, 80);
  printf("The name of source file is %s\n", infilename);
  if ((infile = fopen(infilename, "r")) == nullptr) {
    printf("File %s can't be opened.\n", infilename);
    exit(1);
  }
  init_front_end();

  test8();

  getchar();
}

int main() { testFunc(); }

int showSymbol(uint64 s) {
  if (s == 0)
    return 0;
  for (auto i = 0;; i += 1) {
    s >>= 1;
    if (s == 0)
      return i + 1;
  }
}
