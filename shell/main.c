/*
** This source file is part of MY-BASIC
**
** For the latest info, see https://github.com/paladin-t/my_basic/
**
** Copyright (C) 2011 - 2016 Wang Renxin
**
** Permission is hereby granted, free of charge, to any person obtaining a copy of
** this software and associated documentation files (the "Software"), to deal in
** the Software without restriction, including without limitation the rights to
** use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
** the Software, and to permit persons to whom the Software is furnished to do so,
** subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in all
** copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
** FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
** COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
** IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
** CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifdef _MSC_VER
#	ifndef _CRT_SECURE_NO_WARNINGS
#		define _CRT_SECURE_NO_WARNINGS
#	endif /* _CRT_SECURE_NO_WARNINGS */
#endif /* _MSC_VER */

#include "../core/my_basic.h"
#ifdef _MSC_VER
#	include <crtdbg.h>
#	include <conio.h>
#	include <Windows.h>
#elif !defined __BORLANDC__ && !defined __TINYC__
#	include <unistd.h>
#endif /* _MSC_VER */
#ifndef _MSC_VER
#	include <stdint.h>
#endif /* _MSC_VER */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#ifdef __APPLE__
#	include <sys/time.h>
#endif /* __APPLE__ */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef _MSC_VER
#	pragma warning(disable : 4127)
#	pragma warning(disable : 4706)
#	pragma warning(disable : 4996)
#endif /* _MSC_VER */

#ifdef __BORLANDC__
#	pragma warn -8004
#	pragma warn -8008
#	pragma warn -8066
#endif /* __BORLANDC__ */

#ifdef __POCC__
#	define strdup _strdup
#	define unlink _unlink
#endif /* __POCC__ */

/*
** {========================================================
** Common declarations
*/

#ifdef _MSC_VER
#	define _BIN_FILE_NAME "my_basic"
#elif defined __APPLE__
#	define _BIN_FILE_NAME "my_basic_mac"
#else /* _MSC_VER */
#	define _BIN_FILE_NAME "my_basic_bin"
#endif /* _MSC_VER */

/* Define as 1 to use memory pool, 0 to disable */
#define _USE_MEM_POOL 1

#define _MAX_LINE_LENGTH 256
#define _str_eq(__str1, __str2) (mb_stricmp((__str1), (__str2)) == 0)

#define _LINE_INC_STEP 16

#define _NO_END(s) ((s) == MB_FUNC_OK || (s) == MB_FUNC_SUSPEND || (s) == MB_FUNC_WARNING || (s) == MB_FUNC_ERR || (s) == MB_FUNC_END)

static struct mb_interpreter_t* bas = 0;

/* ========================================================} */

/*
** {========================================================
** Common
*/

#ifndef _countof
#	define _countof(__a) (sizeof(__a) / sizeof(*(__a)))
#endif /* _countof */

#ifndef _printf
#	define _printf printf
#endif /* _printf */

/* ========================================================} */

/*
** {========================================================
** Memory manipulation
*/

#if _USE_MEM_POOL
extern MBAPI const size_t MB_SIZEOF_4BYTES;
extern MBAPI const size_t MB_SIZEOF_8BYTES;
extern MBAPI const size_t MB_SIZEOF_32BYTES;
extern MBAPI const size_t MB_SIZEOF_64BYTES;
extern MBAPI const size_t MB_SIZEOF_128BYTES;
extern MBAPI const size_t MB_SIZEOF_256BYTES;
extern MBAPI const size_t MB_SIZEOF_512BYTES;
extern MBAPI const size_t MB_SIZEOF_INT;
extern MBAPI const size_t MB_SIZEOF_PTR;
extern MBAPI const size_t MB_SIZEOF_LSN;
extern MBAPI const size_t MB_SIZEOF_HTN;
extern MBAPI const size_t MB_SIZEOF_HTA;
extern MBAPI const size_t MB_SIZEOF_OBJ;
extern MBAPI const size_t MB_SIZEOF_FUN;
extern MBAPI const size_t MB_SIZEOF_VAR;
#ifdef MB_ENABLE_USERTYPE_REF
extern MBAPI const size_t MB_SIZEOF_UTR;
#else /* MB_ENABLE_USERTYPE_REF */
static const size_t MB_SIZEOF_UTR = 16;
#endif /* MB_ENABLE_USERTYPE_REF */
extern MBAPI const size_t MB_SIZEOF_ARR;
extern MBAPI const size_t MB_SIZEOF_LBL;
#ifdef MB_ENABLE_COLLECTION_LIB
extern MBAPI const size_t MB_SIZEOF_LST;
extern MBAPI const size_t MB_SIZEOF_DCT;
#else /* MB_ENABLE_COLLECTION_LIB */
static const size_t MB_SIZEOF_LST = 16;
static const size_t MB_SIZEOF_DCT = 16;
#endif /* MB_ENABLE_COLLECTION_LIB */
extern MBAPI const size_t MB_SIZEOF_RTN;
#ifdef MB_ENABLE_CLASS
extern MBAPI const size_t MB_SIZEOF_CLS;
#else /* MB_ENABLE_CLASS */
static const size_t MB_SIZEOF_CLS = 16;
#endif /* MB_ENABLE_CLASS */

typedef unsigned _pool_chunk_size_t;

typedef union _pool_tag_t {
	_pool_chunk_size_t size;
	void* ptr;
} _pool_tag_t;

typedef struct _pool_t {
	size_t size;
	char* stack;
} _pool_t;

static int pool_count = 0;

static _pool_t* pool = 0;

static long alloc_count = 0;
static long alloc_bytes = 0;
static long in_pool_count = 0;
static long in_pool_bytes = 0;

static long _POOL_THRESHOLD_COUNT = 0;
static long _POOL_THRESHOLD_BYTES = 1024 * 1024 * 32;

#define _POOL_NODE_ALLOC(size) (((char*)malloc(sizeof(_pool_tag_t) + size)) + sizeof(_pool_tag_t))
#define _POOL_NODE_PTR(s) (s - sizeof(_pool_tag_t))
#define _POOL_NODE_NEXT(s) (*((void**)(s - sizeof(_pool_tag_t))))
#define _POOL_NODE_SIZE(s) (*((_pool_chunk_size_t*)(s - sizeof(_pool_tag_t))))
#define _POOL_NODE_FREE(s) do { free(_POOL_NODE_PTR(s)); } while(0)

static int _cmp_size_t(const void* l, const void* r) {
	size_t* pl = (size_t*)l;
	size_t* pr = (size_t*)r;

	if(*pl > *pr) return 1;
	else if(*pl < *pr) return -1;
	else return 0;
}

static void _tidy_mem_pool(bool_t force) {
	int i = 0;
	char* s = 0;

	if(!force) {
		if(_POOL_THRESHOLD_COUNT > 0 && in_pool_count < _POOL_THRESHOLD_COUNT)
			return;

		if(_POOL_THRESHOLD_BYTES > 0 && in_pool_bytes < _POOL_THRESHOLD_BYTES)
			return;
	}

	if(!pool_count)
		return;

	for(i = 0; i < pool_count; i++) {
		while((s = pool[i].stack)) {
			pool[i].stack = (char*)_POOL_NODE_NEXT(s);
			_POOL_NODE_FREE(s);
		}
	}

	in_pool_count = 0;
	in_pool_bytes = 0;
}

static void _open_mem_pool(void) {
#define N 22
	size_t szs[N];
	size_t lst[N];
	int i = 0;
	int j = 0;
	size_t s = 0;

	pool_count = 0;

	szs[i++] = MB_SIZEOF_4BYTES;
	szs[i++] = MB_SIZEOF_8BYTES;
	szs[i++] = MB_SIZEOF_32BYTES;
	szs[i++] = MB_SIZEOF_64BYTES;
	szs[i++] = MB_SIZEOF_128BYTES;
	szs[i++] = MB_SIZEOF_256BYTES;
	szs[i++] = MB_SIZEOF_512BYTES;
	szs[i++] = MB_SIZEOF_INT;
	szs[i++] = MB_SIZEOF_PTR;
	szs[i++] = MB_SIZEOF_LSN;
	szs[i++] = MB_SIZEOF_HTN;
	szs[i++] = MB_SIZEOF_HTA;
	szs[i++] = MB_SIZEOF_OBJ;
	szs[i++] = MB_SIZEOF_FUN;
	szs[i++] = MB_SIZEOF_VAR;
	szs[i++] = MB_SIZEOF_UTR;
	szs[i++] = MB_SIZEOF_ARR;
	szs[i++] = MB_SIZEOF_LBL;
	szs[i++] = MB_SIZEOF_LST;
	szs[i++] = MB_SIZEOF_DCT;
	szs[i++] = MB_SIZEOF_RTN;
	szs[i++] = MB_SIZEOF_CLS;

	mb_assert(i == N);

	memset(lst, 0, sizeof(lst));

	/* Find all unduplicated sizes */
	for(i = 0; i < N; i++) {
		s = szs[i];
		for(j = 0; j < N; j++) {
			if(!lst[j]) {
				lst[j] = s;
				pool_count++;

				break;
			} else if(lst[j] == s) {
				break;
			}
		}
	}
	qsort(lst, pool_count, sizeof(lst[0]), _cmp_size_t);

	pool = (_pool_t*)malloc(sizeof(_pool_t) * pool_count);
	for(i = 0; i < pool_count; i++) {
		pool[i].size = lst[i];
		pool[i].stack = 0;
	}
#undef N
}

static void _close_mem_pool(void) {
	int i = 0;
	char* s = 0;

	if(!pool_count)
		return;

	for(i = 0; i < pool_count; i++) {
		while((s = pool[i].stack)) {
			pool[i].stack = (char*)_POOL_NODE_NEXT(s);
			_POOL_NODE_FREE(s);
		}
	}

	free((void*)pool);
	pool = 0;
	pool_count = 0;
}

static char* _pop_mem(unsigned s) {
	int i = 0;
	_pool_t* pl = 0;
	char* result = 0;

	++alloc_count;
	alloc_bytes += s;

	if(pool_count) {
		for(i = 0; i < pool_count; i++) {
			pl = &pool[i];
			if(s <= pl->size) {
				if(pl->stack) {
					in_pool_count--;
					in_pool_bytes -= (long)(_pool_chunk_size_t)s;

					/* Pop from stack */
					result = pl->stack;
					pl->stack = (char*)_POOL_NODE_NEXT(result);
					_POOL_NODE_SIZE(result) = (_pool_chunk_size_t)s;

					return result;
				} else {
					/* Create a new node */
					result = _POOL_NODE_ALLOC(pl->size);
					if((intptr_t)result == sizeof(_pool_tag_t)) {
						result = 0;
					} else {
						_POOL_NODE_SIZE(result) = (_pool_chunk_size_t)s;
					}

					return result;
				}
			}
		}
	}

	/* Allocate directly */
	result = _POOL_NODE_ALLOC(s);
	if((intptr_t)result == sizeof(_pool_tag_t)) {
		result = 0;
	} else {
		_POOL_NODE_SIZE(result) = (_pool_chunk_size_t)s;
	}

	return result;
}

static void _push_mem(char* p) {
	int i = 0;
	_pool_t* pl = 0;

	if(--alloc_count < 0) {
		mb_assert(0 && "Multiple free.");
	}
	alloc_bytes -= _POOL_NODE_SIZE(p);

	if(pool_count) {
		for(i = 0; i < pool_count; i++) {
			pl = &pool[i];
			if(_POOL_NODE_SIZE(p) <= pl->size) {
				in_pool_count++;
				in_pool_bytes += _POOL_NODE_SIZE(p);

				/* Push to stack */
				_POOL_NODE_NEXT(p) = pl->stack;
				pl->stack = p;

				_tidy_mem_pool(false);

				return;
			}
		}
	}

	/* Free directly */
	_POOL_NODE_FREE(p);
}
#endif /* _USE_MEM_POOL */

/* ========================================================} */

/*
** {========================================================
** Code manipulation
*/

typedef struct _code_line_t {
	char** lines;
	int count;
	int size;
} _code_line_t;

static _code_line_t* code = 0;

static _code_line_t* _code(void) {
	return code;
}

static _code_line_t* _create_code(void) {
	_code_line_t* result = (_code_line_t*)malloc(sizeof(_code_line_t));
	result->count = 0;
	result->size = _LINE_INC_STEP;
	result->lines = (char**)malloc(sizeof(char*) * result->size);

	code = result;

	return result;
}

static void _destroy_code(void) {
	int i = 0;

	mb_assert(code);

	for(i = 0; i < code->count; ++i)
		free(code->lines[i]);
	free(code->lines);
	free(code);
}

static void _clear_code(void) {
	int i = 0;

	mb_assert(code);

	for(i = 0; i < code->count; ++i)
		free(code->lines[i]);
	code->count = 0;
}

static void _append_line(char* txt) {
	int l = 0;
	char* buf = 0;

	mb_assert(code && txt);

	if(code->count + 1 == code->size) {
		code->size += _LINE_INC_STEP;
		code->lines = (char**)realloc(code->lines, sizeof(char*) * code->size);
	}
	l = (int)strlen(txt);
	buf = (char*)malloc(l + 2);
	memcpy(buf, txt, l);
	buf[l] = '\n';
	buf[l + 1] = '\0';
	code->lines[code->count++] = buf;
}

static char* _get_code(void) {
	char* result = 0;
	int i = 0;

	mb_assert(code);

	result = (char*)malloc((_MAX_LINE_LENGTH + 2) * code->count + 1);
	result[0] = '\0';
	for(i = 0; i < code->count; ++i) {
		result = strcat(result, code->lines[i]);
		if(i != code->count - 1)
			result = strcat(result, "\n");
	}

	return result;
}

static void _set_code(char* txt) {
	char* cursor = 0;
	char _c = '\0';

	mb_assert(code);

	if(!txt)
		return;

	_clear_code();
	cursor = txt;
	do {
		_c = *cursor;
		if(_c == '\r' || _c == '\n' || _c == '\0') {
			cursor[0] = '\0';
			if(_c == '\r' && *(cursor + 1) == '\n')
				++cursor;
			_append_line(txt);
			txt = cursor + 1;
		}
		++cursor;
	} while(_c);
}

static char* _load_file(const char* path) {
	FILE* fp = 0;
	char* result = 0;
	long curpos = 0;
	long l = 0;

	mb_assert(path);

	fp = fopen(path, "rb");
	if(fp) {
		curpos = ftell(fp);
		fseek(fp, 0L, SEEK_END);
		l = ftell(fp);
		fseek(fp, curpos, SEEK_SET);
		result = (char*)malloc((size_t)(l + 1));
		mb_assert(result);
		fread(result, 1, l, fp);
		fclose(fp);
		result[l] = '\0';
	}

	return result;
}

static int _save_file(const char* path, const char* txt) {
	FILE* fp = 0;

	mb_assert(path && txt);

	fp = fopen(path, "wb");
	if(fp) {
		fwrite(txt, sizeof(char), strlen(txt), fp);
		fclose(fp);

		return 1;
	}

	return 0;
}

/* ========================================================} */

/*
** {========================================================
** Importing directories
*/

typedef struct _importing_dirs_t {
	char** dirs;
	int count;
	int size;
} _importing_dirs_t;

static _importing_dirs_t* importing_dirs = 0;

static void _destroy_importing_directories(void) {
	int i = 0;

	if(!importing_dirs) return;

	for(i = 0; i < importing_dirs->count; ++i)
		free(importing_dirs->dirs[i]);
	free(importing_dirs->dirs);
	free(importing_dirs);
}

static _importing_dirs_t* _set_importing_directories(char* dirs) {
	if(dirs) {
		char* end = dirs + strlen(dirs);
		_importing_dirs_t* result = (_importing_dirs_t*)malloc(sizeof(_importing_dirs_t));
		result->count = 0;
		result->size = _LINE_INC_STEP;
		result->dirs = (char**)malloc(sizeof(char*) * result->size);

		while(dirs && dirs < end && *dirs) {
			int l = 0;
			char* buf = 0;
			bool_t as = false;
			strtok(dirs, ";");
			if(!(*dirs)) continue;
			if(*dirs == ';') { dirs++; continue; }
			if(result->count + 1 == result->size) {
				result->size += _LINE_INC_STEP;
				result->dirs = (char**)realloc(result->dirs, sizeof(char*) * result->size);
			}
			l = (int)strlen(dirs);
			as = dirs[l - 1] != '/' && dirs[l - 1] != '\\';
			buf = (char*)malloc(l + (as ? 2 : 1));
			memcpy(buf, dirs, l);
			if(as) {
				buf[l] = '/';
				buf[l + 1] = '\0';
			} else {
				buf[l] = '\0';
			}
			result->dirs[result->count++] = buf;
			while(*buf) {
				if(*buf == '\\') *buf = '/';
				buf++;
			}
			dirs += l + 1;
		}

		_destroy_importing_directories();
		importing_dirs = result;

		return result;
	}

	return 0;
}

static bool_t _try_import(struct mb_interpreter_t* s, const char* p) {
	bool_t result = false;
	int i = 0;
	mb_unrefvar(s);

	for(i = 0; i < importing_dirs->count; i++) {
		char* t = 0;
		char* d = importing_dirs->dirs[i];
		int m = (int)strlen(d);
		int n = (int)strlen(p);
#if _USE_MEM_POOL
		char* buf = _pop_mem(m + n + 1);
#else /* _USE_MEM_POOL */
		char* buf = (char*)malloc(m + n + 1);
#endif /* _USE_MEM_POOL */
		memcpy(buf, d, m);
		memcpy(buf + m, p, n);
		buf[m + n] = '\0';
		t = _load_file(buf);
		if(t) {
			if(mb_load_string(bas, t, true) == MB_FUNC_OK)
				result = true;
			free(t);
		}
#if _USE_MEM_POOL
		_push_mem(buf);
#else /* _USE_MEM_POOL */
		free(buf);
#endif /* _USE_MEM_POOL */
		if(result)
			break;
	}

	return result;
}

/* ========================================================} */

/*
** {========================================================
** Interactive commands
*/

static void _clear_screen(void) {
#ifdef _MSC_VER
	system("cls");
#else /* _MSC_VER */
	system("clear");
#endif /* _MSC_VER */
}

static int _new_program(void) {
	int result = 0;

	_clear_code();

	result = mb_reset(&bas, false);

	mb_gc(bas, 0);

#if _USE_MEM_POOL
	_tidy_mem_pool(true);
#endif /* _USE_MEM_POOL */

	return result;
}

static void _list_program(const char* sn, const char* cn) {
	long lsn = 0;
	long lcn = 0;

	mb_assert(sn && cn);

	lsn = atoi(sn);
	lcn = atoi(cn);
	if(lsn == 0 && lcn == 0) {
		long i = 0;
		for(i = 0; i < _code()->count; ++i) {
			_printf("%ld]%s", i + 1, _code()->lines[i]);
		}
	} else {
		long i = 0;
		long e = 0;
		if(lsn < 1 || lsn > _code()->count) {
			_printf("Line number %ld out of bound.\n", lsn);

			return;
		}
		if(lcn < 0) {
			_printf("Invalid line count %ld.\n", lcn);

			return;
		}
		--lsn;
		e = lcn ? lsn + lcn : _code()->count;
		for(i = lsn; i < e; ++i) {
			if(i >= _code()->count)
				break;

			_printf("%ld]%s\n", i + 1, _code()->lines[i]);
		}
	}
}

static void _edit_program(const char* no) {
	char line[_MAX_LINE_LENGTH];
	long lno = 0;
	int l = 0;

	mb_assert(no);

	lno = atoi(no);
	if(lno < 1 || lno > _code()->count) {
		_printf("Line number %ld out of bound.\n", lno);

		return;
	}
	--lno;
	memset(line, 0, _MAX_LINE_LENGTH);
	_printf("%ld]", lno + 1);
	mb_gets(line, _MAX_LINE_LENGTH);
	l = (int)strlen(line);
	_code()->lines[lno] = (char*)realloc(_code()->lines[lno], l + 2);
	strcpy(_code()->lines[lno], line);
	_code()->lines[lno][l] = '\n';
	_code()->lines[lno][l + 1] = '\0';
}

static void _insert_program(const char* no) {
	char line[_MAX_LINE_LENGTH];
	long lno = 0;
	int l = 0;
	int i = 0;

	mb_assert(no);

	lno = atoi(no);
	if(lno < 1 || lno > _code()->count) {
		_printf("Line number %ld out of bound.\n", lno);

		return;
	}
	--lno;
	memset(line, 0, _MAX_LINE_LENGTH);
	_printf("%ld]", lno + 1);
	mb_gets(line, _MAX_LINE_LENGTH);
	if(_code()->count + 1 == _code()->size) {
		_code()->size += _LINE_INC_STEP;
		_code()->lines = (char**)realloc(_code()->lines, sizeof(char*) * _code()->size);
	}
	for(i = _code()->count; i > lno; i--)
		_code()->lines[i] = _code()->lines[i - 1];
	l = (int)strlen(line);
	_code()->lines[lno] = (char*)realloc(0, l + 2);
	strcpy(_code()->lines[lno], line);
	_code()->lines[lno][l] = '\n';
	_code()->lines[lno][l + 1] = '\0';
	_code()->count++;
}

static void _alter_program(const char* no) {
	long lno = 0;
	long i = 0;

	mb_assert(no);

	lno = atoi(no);
	if(lno < 1 || lno > _code()->count) {
		_printf("Line number %ld out of bound.\n", lno);

		return;
	}
	--lno;
	free(_code()->lines[lno]);
	for(i = lno; i < _code()->count - 1; i++)
		_code()->lines[i] = _code()->lines[i + 1];
	_code()->count--;
}

static void _load_program(const char* path) {
	char* txt = _load_file(path);
	if(txt) {
		_new_program();
		_set_code(txt);
		free(txt);
		if(_code()->count == 1) {
			_printf("Load done. %d line loaded.\n", _code()->count);
		} else {
			_printf("Load done. %d lines loaded.\n", _code()->count);
		}
	} else {
		_printf("Cannot load file \"%s\".\n", path);
	}
}

static void _save_program(const char* path) {
	char* txt = _get_code();
	if(!_save_file(path, txt)) {
		_printf("Cannot save file \"%s\".\n", path);
	} else {
		if(_code()->count == 1) {
			_printf("Save done. %d line saved.\n", _code()->count);
		} else {
			_printf("Save done. %d lines saved.\n", _code()->count);
		}
	}
	free(txt);
}

static void _kill_program(const char* path) {
	if(!unlink(path)) {
		_printf("Delete file \"%s\" successfully.\n", path);
	} else {
		_printf("Delete file \"%s\" failed.\n", path);
	}
}

static void _list_directory(const char* path) {
	char line[_MAX_LINE_LENGTH];
#ifdef _MSC_VER
	if(path && *path) sprintf(line, "dir %s", path);
	else sprintf(line, "dir");
#else /* _MSC_VER */
	if(path && *path) sprintf(line, "ls %s", path);
	else sprintf(line, "ls");
#endif /* _MSC_VER */
	system(line);
}

static void _show_tip(void) {
	_printf("MY-BASIC Interpreter Shell - %s\n", mb_ver_string());
	_printf("Copyright (C) 2011 - 2016 Wang Renxin. All Rights Reserved.\n");
	_printf("For more information, see https://github.com/paladin-t/my_basic/.\n");
	_printf("Input HELP and hint enter to view help information.\n");
}

static void _show_help(void) {
	_printf("Modes:\n");
	_printf("  %s           - Start interactive mode without arguments\n", _BIN_FILE_NAME);
	_printf("  %s *.*       - Load and run a file\n", _BIN_FILE_NAME);
	_printf("  %s -e \"expr\" - Evaluate an expression directly\n", _BIN_FILE_NAME);
	_printf("\n");
	_printf("Options:\n");
#if _USE_MEM_POOL
	_printf("  -p n       - Set memory pool threashold size, n is size in bytes\n");
#endif /* _USE_MEM_POOL */
	_printf("  -f \"dirs\"  - Set importing directories, separated by semicolon\n");
	_printf("\n");
	_printf("Interactive commands:\n");
	_printf("  HELP  - View help information\n");
	_printf("  CLS   - Clear screen\n");
	_printf("  NEW   - Clear current program\n");
	_printf("  RUN   - Run current program\n");
	_printf("  BYE   - Quit interpreter\n");
	_printf("  LIST  - List current program\n");
	_printf("          Usage: LIST [l [n]], l is start line number, n is line count\n");
	_printf("  EDIT  - Edit (modify/insert/remove) a line in current program\n");
	_printf("          Usage: EDIT n, n is line number\n");
	_printf("                 EDIT -i n, insert a line before a given line, n is line number\n");
	_printf("                 EDIT -r n, remove a line, n is line number\n");
	_printf("  LOAD  - Load a file as current program\n");
	_printf("          Usage: LOAD *.*\n");
	_printf("  SAVE  - Save current program to a file\n");
	_printf("          Usage: SAVE *.*\n");
	_printf("  KILL  - Delete a file\n");
	_printf("          Usage: KILL *.*\n");
	_printf("  DIR   - List a directory\n");
	_printf("          Usage: DIR [p], p is a directory path\n");
}

static int _do_line(void) {
	int result = MB_FUNC_OK;
	char line[_MAX_LINE_LENGTH];
	char dup[_MAX_LINE_LENGTH];

	mb_assert(bas);

	memset(line, 0, _MAX_LINE_LENGTH);
	_printf("]");
	mb_gets(line, _MAX_LINE_LENGTH);

	memcpy(dup, line, _MAX_LINE_LENGTH);
	strtok(line, " ");

	if(_str_eq(line, "")) {
		/* Do nothing */
	} else if(_str_eq(line, "HELP")) {
		_show_help();
	} else if(_str_eq(line, "CLS")) {
		_clear_screen();
	} else if(_str_eq(line, "NEW")) {
		result = _new_program();
	} else if(_str_eq(line, "RUN")) {
		int i = 0;
		mb_assert(_code());
		result = mb_reset(&bas, false);
		for(i = 0; i < _code()->count; ++i) {
			if(result)
				break;

			result = mb_load_string(bas, _code()->lines[i], false);
		}
		if(result == MB_FUNC_OK)
			result = mb_run(bas);
		_printf("\n");
	} else if(_str_eq(line, "BYE")) {
		result = MB_FUNC_BYE;
	} else if(_str_eq(line, "LIST")) {
		char* sn = line + strlen(line) + 1;
		char* cn = 0;
		strtok(sn, " ");
		cn = sn + strlen(sn) + 1;
		_list_program(sn, cn);
	} else if(_str_eq(line, "EDIT")) {
		char* no = line + strlen(line) + 1;
		char* ne = 0;
		strtok(no, " ");
		ne = no + strlen(no) + 1;
		if(!(*ne))
			_edit_program(no);
		else if(_str_eq(no, "-I"))
			_insert_program(ne);
		else if(_str_eq(no, "-R"))
			_alter_program(ne);
	} else if(_str_eq(line, "LOAD")) {
		char* path = line + strlen(line) + 1;
		_load_program(path);
	} else if(_str_eq(line, "SAVE")) {
		char* path = line + strlen(line) + 1;
		_save_program(path);
	} else if(_str_eq(line, "KILL")) {
		char* path = line + strlen(line) + 1;
		_kill_program(path);
	} else if(_str_eq(line, "DIR")) {
		char* path = line + strlen(line) + 1;
		_list_directory(path);
	} else {
		_append_line(dup);
	}

	return result;
}

/* ========================================================} */

/*
** {========================================================
** Parameter processing
*/

#define _CHECK_ARG(__c, __i, __e) \
	do { \
		if(__c <= __i + 1) { \
			_printf(__e); \
			return true; \
		} \
	} while(0)

static void _run_file(char* path) {
	if(mb_load_file(bas, path) == MB_FUNC_OK) {
		mb_run(bas);
	} else {
		_printf("Invalid file or wrong program.\n");
	}
}

static void _evaluate_expression(char* p) {
	char pr[8];
	int l = 0;
	int k = 0;
	bool_t a = true;
	char* e = 0;

	const char* const print = "PRINT ";

	if(!p) {
		_printf("Invalid expression.\n");

		return;
	}

	l = (int)strlen(p);
	k = (int)strlen(print);
	if(l >= k) {
		memcpy(pr, p, k);
		pr[k] = '\0';
		if(_str_eq(pr, print))
			a = false;
	}
	if(a) {
		e = (char*)malloc(l + k + 1);
		memcpy(e, print, k);
		memcpy(e + k, p, l);
		e[l + k] = '\0';
		p = e;
	}
	if(mb_load_string(bas, p, true) == MB_FUNC_OK) {
		mb_run(bas);
	} else {
		_printf("Invalid expression.\n");
	}
	if(a)
		free(e);
}

static bool_t _process_parameters(int argc, char* argv[]) {
	int i = 1;
	char* prog = 0;
	bool_t eval = false;
	char* memp = 0;
	char* diri = 0;

	while(i < argc) {
		if(!memcmp(argv[i], "-", 1)) {
			if(!memcmp(argv[i] + 1, "e", 1)) {
				eval = true;
				_CHECK_ARG(argc, i, "-e: Expression expected.\n");
				prog = argv[++i];
#if _USE_MEM_POOL
			} else if(!memcmp(argv[i] + 1, "p", 1)) {
				_CHECK_ARG(argc, i, "-p: Memory pool threashold size expected.\n");
				memp = argv[++i];
				if(argc > i + 1)
					prog = argv[++i];
#endif /* _USE_MEM_POOL */
			} else if(!memcmp(argv[i] + 1, "f", 1)) {
				_CHECK_ARG(argc, i, "-f: Importing directories expected.\n");
				diri = argv[++i];
			} else {
				_printf("Unknown argument: %s.\n", argv[i]);
			}
		} else {
			prog = argv[i];
		}

		i++;
	}

#if _USE_MEM_POOL
	if(memp)
		_POOL_THRESHOLD_BYTES = atoi(memp);
#else /* _USE_MEM_POOL */
	mb_unrefvar(memp);
#endif /* _USE_MEM_POOL */
	if(diri)
		_set_importing_directories(diri);
	if(eval)
		_evaluate_expression(prog);
	else if(prog)
		_run_file(prog);
	else
		return false;

	return true;
}

/* ========================================================} */

/*
** {========================================================
** Scripting interfaces
*/

#define _HAS_TICKS
#if defined _MSC_VER
static int_t _ticks(void) {
	LARGE_INTEGER li;
	double freq = 0.0;
	int_t ret = 0;

	QueryPerformanceFrequency(&li);
	freq = (double)li.QuadPart / 1000.0;
	QueryPerformanceCounter(&li);
	ret = (int_t)((double)li.QuadPart / freq);

	return ret;
}
#elif defined __APPLE__
static int_t _ticks(void) {
	struct timespec ts;
	struct timeval now;
	int rv = 0;

	rv = gettimeofday(&now, 0);
	if(rv) return 0;
	ts.tv_sec  = now.tv_sec;
	ts.tv_nsec = now.tv_usec * 1000;

	return (int_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}
#elif defined __GNUC__
static int_t _ticks(void) {
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);

	return (int_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}
#else /* _MSC_VER */
#	undef _HAS_TICKS
#endif /* _MSC_VER */

#ifdef _HAS_TICKS
static int ticks(struct mb_interpreter_t* s, void** l) {
	int result = MB_FUNC_OK;

	mb_assert(s && l);

	mb_check(mb_attempt_open_bracket(s, l));

	mb_check(mb_attempt_close_bracket(s, l));

	mb_check(mb_push_int(s, l, _ticks()));

	return result;
}
#endif /* _HAS_TICKS */

static int now(struct mb_interpreter_t* s, void** l) {
	int result = MB_FUNC_OK;
	time_t ct;
	struct tm* timeinfo;
	char buf[80];
	char* arg = 0;

	mb_assert(s && l);

	mb_check(mb_attempt_open_bracket(s, l));

	if(mb_has_arg(s, l)) {
		mb_check(mb_pop_string(s, l, &arg));
	}

	mb_check(mb_attempt_close_bracket(s, l));

	time(&ct);
	timeinfo = localtime(&ct);
	if(arg) {
		strftime(buf, _countof(buf), arg, timeinfo);
		mb_check(mb_push_string(s, l, mb_memdup(buf, (unsigned)(strlen(buf) + 1))));
	} else {
		arg = asctime(timeinfo);
		mb_check(mb_push_string(s, l, mb_memdup(arg, (unsigned)(strlen(arg) + 1))));
	}

	return result;
}

static int set_importing_dirs(struct mb_interpreter_t* s, void** l) {
	int result = MB_FUNC_OK;
	char* arg = 0;

	mb_assert(s && l);

	mb_check(mb_attempt_open_bracket(s, l));

	mb_check(mb_pop_string(s, l, &arg));
	if(arg)
		_set_importing_directories(arg);

	mb_check(mb_attempt_close_bracket(s, l));

	return result;
}

static int sys(struct mb_interpreter_t* s, void** l) {
	int result = MB_FUNC_OK;
	char* arg = 0;

	mb_assert(s && l);

	mb_check(mb_attempt_open_bracket(s, l));

	mb_check(mb_pop_string(s, l, &arg));

	mb_check(mb_attempt_close_bracket(s, l));

	if(arg)
		system(arg);

	return result;
}

static int trace(struct mb_interpreter_t* s, void** l) {
	int result = MB_FUNC_OK;
	char* frames[16];
	char** p = frames;

	mb_assert(s && l);

	memset(frames, 0, _countof(frames));

	mb_check(mb_attempt_open_bracket(s, l));

	mb_check(mb_attempt_close_bracket(s, l));

	mb_check(mb_debug_get_stack_trace(s, l, frames, _countof(frames)));

	++p;
	while(*p) {
		_printf("%s", *p);
		++p;
		if(*p) {
			_printf(" <- ");
		}
	}

	return result;
}

static int raise(struct mb_interpreter_t* s, void** l) {
	int result = MB_EXTENDED_ABORT;
	int_t err = 0;

	mb_assert(s && l);

	mb_check(mb_attempt_open_bracket(s, l));

	if(mb_has_arg(s, l)) {
		mb_check(mb_pop_int(s, l, &err));
	}

	mb_check(mb_attempt_close_bracket(s, l));

	mb_check(mb_push_int(s, l, err));

	result = mb_raise_error(s, l, SE_EA_EXTENDED_ABORT, MB_EXTENDED_ABORT + err);

	return result;
}

static int gc(struct mb_interpreter_t* s, void** l) {
	int result = MB_FUNC_OK;
	int_t collected = 0;

	mb_assert(s && l);

	mb_check(mb_attempt_open_bracket(s, l));

	mb_check(mb_attempt_close_bracket(s, l));

	mb_check(mb_gc(s, &collected));

	mb_check(mb_push_int(s, l, collected));

	return result;
}

static int beep(struct mb_interpreter_t* s, void** l) {
	int result = MB_FUNC_OK;

	mb_assert(s && l);

	mb_check(mb_attempt_func_begin(s, l));

	mb_check(mb_attempt_func_end(s, l));

	putchar('\a');

	return result;
}

/* ========================================================} */

/*
** {========================================================
** Callbacks and handlers
*/

static void _on_stepped(struct mb_interpreter_t* s, char* f, int p, unsigned short row, unsigned short col) {
	mb_unrefvar(s);
	mb_unrefvar(f);
	mb_unrefvar(p);
	mb_unrefvar(row);
	mb_unrefvar(col);
}

static void _on_error(struct mb_interpreter_t* s, mb_error_e e, char* m, char* f, int p, unsigned short row, unsigned short col, int abort_code) {
	mb_unrefvar(s);
	mb_unrefvar(p);

	if(SE_NO_ERR != e) {
		if(f) {
			_printf(
				"Error:\n    Line %d, Col %d in File: %s\n    Code %d, Abort Code %d\n    Message: %s.\n",
				row, col, f,
				e, e == SE_EA_EXTENDED_ABORT ? abort_code - MB_EXTENDED_ABORT : abort_code,
				m
			);
		} else {
			_printf(
				"Error:\n    Line %d, Col %d\n    Code %d, Abort Code %d\n    Message: %s.\n",
				row, col,
				e, e == SE_EA_EXTENDED_ABORT ? abort_code - MB_EXTENDED_ABORT : abort_code,
				m
			);
		}
	}
}

static int _on_import(struct mb_interpreter_t* s, const char* p) {
	if(!importing_dirs)
		return MB_FUNC_ERR;

	if(!_try_import(s, p))
		return MB_FUNC_ERR;

	return MB_FUNC_OK;
}

/* ========================================================} */

/*
** {========================================================
** Initialization and disposing
*/

static void _on_startup(void) {
#if _USE_MEM_POOL
	_open_mem_pool();

	mb_set_memory_manager(_pop_mem, _push_mem);
#endif /* _USE_MEM_POOL */

	_create_code();

#ifdef _HAS_TICKS
	srand((unsigned)_ticks());
#endif /* _HAS_TICKS */

	mb_init();

	mb_open(&bas);

	mb_debug_set_stepped_handler(bas, _on_stepped);
	mb_set_error_handler(bas, _on_error);
	mb_set_import_handler(bas, _on_import);

#ifdef _HAS_TICKS
	mb_reg_fun(bas, ticks);
#endif /* _HAS_TICKS */
	mb_reg_fun(bas, now);
	mb_reg_fun(bas, set_importing_dirs);
	mb_reg_fun(bas, sys);
	mb_reg_fun(bas, trace);
	mb_reg_fun(bas, raise);
	mb_reg_fun(bas, gc);
	mb_reg_fun(bas, beep);
}

static void _on_exit(void) {
	_destroy_importing_directories();

	mb_close(&bas);

	mb_dispose();

	_destroy_code();

#if _USE_MEM_POOL
	_close_mem_pool();
#endif /* _USE_MEM_POOL */

#if defined _MSC_VER && !defined _WIN64
	if(!!_CrtDumpMemoryLeaks()) { _asm { int 3 } }
#elif _USE_MEM_POOL
	if(alloc_count > 0 || alloc_bytes > 0) { mb_assert(0 && "Memory leak."); }
#endif /* _MSC_VER && !_WIN64 */
}

/* ========================================================} */

/*
** {========================================================
** Entry
*/

int main(int argc, char* argv[]) {
	int status = 0;

#if defined _MSC_VER && !defined _WIN64
	_CrtSetBreakAlloc(0);
#endif /* _MSC_VER && !_WIN64 */

	atexit(_on_exit);

	_on_startup();

	if(argc >= 2) {
		if(!_process_parameters(argc, argv))
			argc = 1;
	}
	if(argc == 1) {
		_show_tip();
		do {
			status = _do_line();
		} while(_NO_END(status));
	}

	return 0;
}

/* ========================================================} */

#ifdef __cplusplus
}
#endif /* __cplusplus */
