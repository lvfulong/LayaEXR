#ifndef _LAYA_EXR_H_
#define _LAYA_EXR_H_
#if defined (EXPORTBUILD)    
# define _DLLExport __declspec (dllexport)    
# else    
# define _DLLExport __declspec (dllimport)    
#endif  

extern "C" void _DLLExport  EXR2PNG(const char* exrPath, const char* pngPath);

#endif // ! _LAYA_EXR_H_

