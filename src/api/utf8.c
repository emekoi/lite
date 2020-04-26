#include <stdlib.h>
#include <string.h>
#include "api.h"
#include "xalloc.h"

#ifdef _WIN32
#include <io.h>
#endif

const char* utf8_decode(const char* text, unsigned* cp) {
	unsigned char c = *text++;
	int extra = 0, min = 0;
	*cp = 0;
 if (c >= 0xF0) { *cp = c & 0x07; extra = 3; min = 0x10000; }
	else if (c >= 0xE0) { *cp = c & 0x0F; extra = 2; min = 0x800; }
	else if (c >= 0xC0) { *cp = c & 0x1F; extra = 1; min = 0x80; }
	else if (c >= 0x80) { *cp = 0xFFFD; }
	else *cp = c;
	while (extra--) {
		c = *text++;
		if ((c & 0xC0) != 0x80) { *cp = 0xFFFD; break; }
		(*cp) = ((*cp) << 6) | (c & 0x3F);
	}
	if (*cp < min) *cp = 0xFFFD;
	return text;
}


char *utf8_encode(char *text, unsigned cp) {
	if (cp < 0 || cp > 0x10FFFF) cp = 0xFFFD;
#define CU_EMIT(X, Y, Z) *text++ = X | ((cp >> Y) & Z)
	if (cp < 0x80) { CU_EMIT(0x00,0,0x7F); }
	else if (cp < 0x800) { CU_EMIT(0xC0,6,0x1F); CU_EMIT(0x80, 0,  0x3F); }
	else if (cp < 0x10000) { CU_EMIT(0xE0,12,0xF); CU_EMIT(0x80, 6,  0x3F); CU_EMIT(0x80, 0, 0x3F); }
	else { CU_EMIT(0xF0,18,0x7); CU_EMIT(0x80, 12, 0x3F); CU_EMIT(0x80, 6, 0x3F); CU_EMIT(0x80, 0, 0x3F); }
	return text;
#undef CU_EMIT
}

size_t utf8_size(unsigned cp) {
	if (cp < 0 || cp > 0x10FFFF) cp = 0xFFFD;
	if (cp < 0x80) return 1;
	else if (cp < 0x800) return 2;
	else if (cp < 0x10000) return 3;
	else return 4;
}


size_t utf8_len(const char *str, size_t len) {
  if (len == 0) return 0;
  const char *p = str;
  size_t result = 0;
  for (;p < str + len; result++) {
      unsigned cp = 0;
      p = utf8_decode(p, &cp);
  }
  return result;
}


wchar_t* utf16_encode(wchar_t* text, unsigned cp) {
	if (cp < 0x10000) *text++ = cp;
	else {
		cp -= 0x10000;
		*text++ = 0xD800 | ((cp >> 10) & 0x03FF);
		*text++ = 0xDC00 | (cp & 0x03FF);
	}
	return text;
}


const wchar_t *utf8_widen(const char* in, wchar_t* out) {
	unsigned cp;
	while (*in) {
		in = utf8_decode(in, &cp);
		out = utf16_encode(out, cp);
	}
  return out;
}


static int f_utf8_size(lua_State *L) {
  const char *str = luaL_checkstring(L, 1);
  int idx = luaL_optint(L, 2, 0);
  unsigned cp = 0;
  utf8_decode(str + idx, &cp);
  lua_pushnumber(L, utf8_size(cp));
  return 1;
}


static int f_utf8_len(lua_State *L) {
  size_t len = 0; 
  const char *str = luaL_checklstring(L, 1, &len);
  lua_pushnumber(L, utf8_len(str, len));
  return 1;
}


static int f_utf8_sub(lua_State *L) {
  size_t str_len = 0; 
  const char *str = luaL_checklstring(L, 1, &str_len);
  ssize_t start = luaL_checknumber(L, 2);
  ssize_t end = luaL_optint(L, 3, -1);
  
  {
    int l = (end >= 0 && start >= 0) ? 0 : utf8_len(str, str_len);
    start = (start >= 0 ? start : l + start) - 1;
    end = (end >= 0 ? end - 1 : l + end);
  }

  if (start > end) {
    lua_pushlstring(L, "", 0);
    return 1;
  }

  const char *p = str;
  const char *s = p;
  for (int i = 0; p < str + str_len; i++) {
    if (start == i) s = p;
    unsigned cp = 0;
    p = utf8_decode(p, &cp);
    if (end == i) break;
  }

  lua_pushlstring(L, s, p - s);
  return 1;
}


static int f_utf8_show(lua_State *L) {
  _setmode(_fileno(stdout), 0x00040000);
  size_t len = 0; 
  const char *str = luaL_checklstring(L, 1, &len);
  wchar_t *wout = xcalloc(utf8_len(str, len), sizeof(wchar_t));
  memset(wout, 0, utf8_len(str, len) * sizeof(wchar_t));
  wprintf(L"%s\n", utf8_widen(str, wout));
  xfree(wout);
  return 0;
}


static int f_utf8_reverse(lua_State *L) {
  size_t len = 0;
  luaL_Buffer b;

  const char *str = luaL_checklstring(L, 1, &len);
  if (len == 0) {
    lua_pushlstring(L, "", 0);
    goto end;
  }
  
  char *out = luaL_buffinitsize(L, &b, len);
  (out += len)[-1] = '\0';

  unsigned cp = 0;
  for (int i = 0; i < len; i += utf8_size(cp)) {
    str = utf8_decode(str, &cp);
    utf8_encode(out - utf8_size(cp), cp);
    out -= utf8_size(cp);
  }

  luaL_pushresultsize(&b, len);
end:
  return 1;
}

// TODO
// - find
// - format
// - gmatch
// - match
// - gsub
// - upper
// - lower


static const luaL_Reg lib[] = {
  { "size",    f_utf8_size    },
  { "len",     f_utf8_len     },
  { "sub",     f_utf8_sub     },
  { "reverse", f_utf8_reverse },
  { "show",    f_utf8_show    },
  { NULL, NULL }
};



int luaopen_utf8(lua_State *L) {  
  luaL_newlib(L, lib);
  return 1;
}
