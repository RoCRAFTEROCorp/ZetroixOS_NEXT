
#define STANDALONE
#include <wine/test.h>

extern void func_MiniDumpSnapshot(void);
extern void func_pdb(void);
extern void func_rsym(void);

const struct test winetest_testlist[] =
{
    { "MiniDumpSnapshot", func_MiniDumpSnapshot },
    { "pdb", func_pdb },
#ifdef _M_IX86
    { "rsym", func_rsym },
#endif
    { 0, 0 }
};
