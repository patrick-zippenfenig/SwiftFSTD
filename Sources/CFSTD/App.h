//
//  Header.h
//  
//
//  Created by Patrick Zippenfenig on 18.04.2024.
//

#ifndef App_h
#define App_h



#define APP_EXTRA 0
#define APP_LIBFST 0
#define APP_LIBFST 0
#define APP_LIBRMN 0
#define APP_ERROR 0
#define APP_DEBUG 0
#define APP_WARNING 1
#define APP_INFO 2
#define APP_FATAL 3
#define APP_VERBATIM 5
void Lib_Log(int Lib, int Level, const char *Format, ...);
int Lib_LogLevel(int Lib, int somtehing);

#endif /* App_h */
