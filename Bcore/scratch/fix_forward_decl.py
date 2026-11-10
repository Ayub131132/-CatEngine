with open(r'c:\Users\ayubg\AndroidStudioProjects\NewBlackbox-main1\NewBlackbox-main\Bcore\src\main\cpp\catEngine\catEngine.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

target = 'static uintptr_t g_libgameBase = 0;'
replacement = '''static uintptr_t g_libgameBase = 0;

extern "C" void CatEngine_UpdateTablePucks();
static inline void updateTablePucks() { CatEngine_UpdateTablePucks(); }'''

assert target in text, 'target not found'
text = text.replace(target, replacement, 1)

with open(r'c:\Users\ayubg\AndroidStudioProjects\NewBlackbox-main1\NewBlackbox-main\Bcore\src\main\cpp\catEngine\catEngine.cpp', 'w', encoding='utf-8') as f:
    f.write(text)

print('Forward declaration added successfully!')
