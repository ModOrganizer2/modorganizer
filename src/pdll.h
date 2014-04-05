////////////////////////////////////////////////////////////////////////////////////////////////////
//  Class:  PDll                                                                                                    //
//  Authors: MicHael Galkovsky                                                                           //
//  Date:    April 14, 1998                                                                                  //
//  Company:  Pervasive Software                                                                    //
//  Purpose:    Base class to wrap dynamic use of dll                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

#if !defined (_PDLL_H_)
#define _PDLL_H_

#include <windows.h>
#include <winbase.h>

#include <windows_error.h>
#include <tchar.h>
#include <sstream>


#define FUNC_LOADED 3456

//function declarations according to the number of parameters
//define the type
//declare a variable of that type
//declare a member function by the same name as the dll function
//check for dll handle
//if this is the first call to the function then try to load it
//if not then if the function was loaded successfully make a call to it
//otherwise return a NULL cast to the return parameter.

#define DECLARE_FUNCTION0(CallType, retVal, FuncName) \
    typedef  retVal (CallType* TYPE_##FuncName)(); \
    TYPE_##FuncName m_##FuncName; \
    short m_is##FuncName; \
    retVal FuncName() \
    { \
        if (m_dllHandle) \
        { \
            if (FUNC_LOADED != m_is##FuncName) \
            {\
                m_##FuncName = NULL; \
                m_##FuncName = (TYPE_##FuncName)GetProcAddress(m_dllHandle, #FuncName); \
                m_is##FuncName = FUNC_LOADED;\
            }\
            if (NULL != m_##FuncName) \
                return m_##FuncName(); \
            else \
                return (retVal)NULL; \
        } \
        else \
            return (retVal)NULL; \
    }

#define DECLARE_FUNCTION1(CallType,retVal, FuncName, Param1) \
    typedef  retVal (CallType* TYPE_##FuncName)(Param1); \
    TYPE_##FuncName m_##FuncName; \
    short m_is##FuncName;\
    retVal FuncName(Param1 p1) \
    { \
        if (m_dllHandle) \
        { \
            if (FUNC_LOADED != m_is##FuncName) \
            {\
                m_##FuncName = NULL; \
                m_##FuncName = (TYPE_##FuncName)GetProcAddress(m_dllHandle, #FuncName); \
                m_is##FuncName = FUNC_LOADED;\
            }\
            if (NULL != m_##FuncName) \
                return m_##FuncName(p1); \
            else \
                return (retVal)NULL; \
        } \
        else \
            return (retVal)NULL; \
    }

#define DECLARE_FUNCTION2(CallType,retVal, FuncName, Param1, Param2) \
    typedef  retVal (CallType* TYPE_##FuncName)(Param1, Param2); \
    TYPE_##FuncName m_##FuncName; \
    short m_is##FuncName;\
    retVal FuncName (Param1 p1, Param2 p2) \
    {\
        if (m_dllHandle)\
        {\
            if (FUNC_LOADED != m_is##FuncName) \
            {\
                m_##FuncName = NULL; \
                m_##FuncName = (TYPE_##FuncName)GetProcAddress(m_dllHandle, #FuncName); \
                m_is##FuncName = FUNC_LOADED;\
            }\
            if (NULL != m_##FuncName) \
                return m_##FuncName(p1, p2); \
            else \
                return (retVal)NULL; \
        } \
        else\
            return (retVal)NULL; \
    }

#define DECLARE_FUNCTION3(CallType,retVal, FuncName, Param1, Param2, Param3) \
    typedef  retVal (CallType* TYPE_##FuncName)(Param1, Param2, Param3); \
    TYPE_##FuncName m_##FuncName; \
    short m_is##FuncName;\
    retVal FuncName (Param1 p1, Param2 p2, Param3 p3) \
    {\
        if (m_dllHandle)\
        {\
            if (FUNC_LOADED != m_is##FuncName) \
            {\
                m_##FuncName = NULL; \
                m_##FuncName = (TYPE_##FuncName)GetProcAddress(m_dllHandle, #FuncName); \
                m_is##FuncName = FUNC_LOADED; \
            }\
            if (NULL != m_##FuncName) \
                return m_##FuncName(p1, p2, p3);\
            else \
                return (retVal)NULL; \
        } \
        else\
            return (retVal)NULL; \
    }

#define DECLARE_FUNCTION4(CallType,retVal, FuncName, Param1, Param2, Param3, Param4) \
    typedef  retVal (CallType* TYPE_##FuncName)(Param1, Param2, Param3, Param4); \
    TYPE_##FuncName m_##FuncName; \
    short m_is##FuncName;\
    retVal FuncName (Param1 p1, Param2 p2, Param3 p3, Param4 p4) \
    {\
        if (m_dllHandle)\
        {\
            if (FUNC_LOADED != m_is##FuncName) \
            {\
                m_##FuncName = NULL; \
                m_##FuncName = (TYPE_##FuncName)GetProcAddress(m_dllHandle, #FuncName); \
                m_is##FuncName = FUNC_LOADED;\
            }\
            if (NULL != m_##FuncName) \
                return m_##FuncName(p1, p2, p3, p4);\
            else \
                return (retVal)NULL; \
        } \
        else\
            return (retVal)NULL; \
    }

#define DECLARE_FUNCTION5(CallType,retVal, FuncName, Param1, Param2, Param3, Param4, Param5) \
    typedef  retVal (CallType* TYPE_##FuncName)(Param1, Param2, Param3, Param4, Param5); \
    TYPE_##FuncName m_##FuncName; \
    short m_is##FuncName; \
    retVal FuncName (Param1 p1, Param2 p2, Param3 p3, Param4 p4, Param5 p5) \
    {\
        if (m_dllHandle)\
        {\
            if (FUNC_LOADED != m_is##FuncName) \
            {\
                m_##FuncName = NULL; \
                m_##FuncName = (TYPE_##FuncName)GetProcAddress(m_dllHandle, #FuncName); \
                m_is##FuncName = FUNC_LOADED;\
            }\
            if (NULL != m_##FuncName) \
                return m_##FuncName(p1, p2, p3, p4, p5);\
            else \
                return (retVal)NULL; \
        } \
        else\
            return (retVal)NULL; \
    }

#define DECLARE_FUNCTION6(CallType,retVal, FuncName, Param1, Param2, Param3, Param4, Param5, Param6) \
    typedef  retVal (CallType* TYPE_##FuncName)(Param1, Param2, Param3, Param4, Param5, Param6); \
    TYPE_##FuncName m_##FuncName; \
    short m_is##FuncName;\
    retVal FuncName (Param1 p1, Param2 p2, Param3 p3, Param4 p4, Param5 p5, Param6 p6) \
    {\
        if (m_dllHandle)\
        {\
            if (FUNC_LOADED != m_is##FuncName) \
            {\
                m_##FuncName = NULL; \
                m_##FuncName = (TYPE_##FuncName)GetProcAddress(m_dllHandle, #FuncName); \
                m_is##FuncName = FUNC_LOADED;\
            }\
            if (NULL != m_##FuncName) \
                return m_##FuncName(p1, p2, p3, p4, p5, p6);\
            else \
                return (retVal)NULL; \
        } \
        else\
            return (retVal)NULL; \
    }

#define DECLARE_FUNCTION7(CallType,retVal, FuncName, Param1, Param2, Param3, Param4, Param5, Param6, Param7) \
    typedef  retVal (CallType* TYPE_##FuncName)(Param1, Param2, Param3, Param4, Param5, Param6, Param7); \
    TYPE_##FuncName m_##FuncName; \
    short m_is##FuncName;\
    retVal FuncName (Param1 p1, Param2 p2, Param3 p3, Param4 p4, Param5 p5, Param6 p6, Param7 p7) \
    {\
        if (m_dllHandle)\
        {\
            if (FUNC_LOADED != m_is##FuncName) \
            {\
                m_##FuncName = NULL; \
                m_##FuncName = (TYPE_##FuncName)GetProcAddress(m_dllHandle, #FuncName); \
                m_is##FuncName = FUNC_LOADED;\
            }\
            if (NULL != m_##FuncName) \
                return m_##FuncName(p1, p2, p3, p4, p5, p6, p7);\
            else \
                return (retVal)NULL; \
        } \
        else\
            return (retVal)NULL; \
    }

#define DECLARE_FUNCTION8(CallType,retVal, FuncName, Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8) \
    typedef  retVal (CallType* TYPE_##FuncName)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8); \
    TYPE_##FuncName m_##FuncName; \
    short m_is##FuncName;\
    retVal FuncName (Param1 p1, Param2 p2, Param3 p3, Param4 p4, Param5 p5, Param6 p6, Param7 p7, Param8 p8) \
    {\
        if (m_dllHandle)\
        {\
            if (FUNC_LOADED != m_is##FuncName) \
            {\
                m_##FuncName = NULL; \
                m_##FuncName = (TYPE_##FuncName)GetProcAddress(m_dllHandle, #FuncName); \
                m_is##FuncName = FUNC_LOADED;\
            }\
            if (NULL != m_##FuncName) \
                return m_##FuncName(p1, p2, p3, p4, p5, p6, p7, p8);\
            else \
                return (retVal)NULL; \
        }\
        else\
            return (retVal)NULL; \
    }

#define DECLARE_FUNCTION9(CallType,retVal, FuncName, Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9) \
    typedef  retVal (CallType* TYPE_##FuncName)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9); \
    TYPE_##FuncName m_##FuncName; \
    short m_is##FuncName; \
    retVal FuncName (Param1 p1, Param2 p2, Param3 p3, Param4 p4, Param5 p5, Param6 p6, Param7 p7, Param8 p8, Param9 p9) \
    {\
        if (m_dllHandle)\
        {\
            if (FUNC_NAME != m_is##FuncName) \
            {\
                m_##FuncName = NULL; \
                m_##FuncName = (TYPE_##FuncName)GetProcAddress(m_dllHandle, #FuncName); \
                m_is##FuncName = FUNC_LOADED;\
            }\
            if (NULL != m_##FuncName) \
                return m_##FuncName(p1, p2, p3, p4, p5, p6, p7, p8, p9);\
            else \
                return (retVal)NULL; \
        }\
        else\
            return (retVal)NULL; \
    }

#define DECLARE_FUNCTION10(CallType,retVal, FuncName, Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10) \
    typedef  retVal (CallType* TYPE_##FuncName)FuncName(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10); \
    TYPE_##FuncName m_##FuncName; \
    short m_is##FuncName;\
    retVal FuncName (Param1 p1, Param2 p2, Param3 p3, Param4 p4, Param5 p5, Param6 p6, Param7 p7, Param8 p8, Param9 p9, Param10 p10) \
    {\
        if (m_dllHandle)\
        {\
            if (FUNC_LOADED != m_is##FuncName) \
            {\
                m_##FuncName = NULL; \
                m_##FuncName = (TYPE_##FuncName)GetProcAddress(m_dllHandle, #FuncName); \
                m_is##FuncName = FUNC_LOADED;\
            }\
            if (NULL != m_##FuncName) \
                return m_##FuncName(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);\
            else \
                return (retVal)NULL; \
        }\
        else                    \
            return (retVal)NULL;\
    }

//declare constructors and LoadFunctions
#define DECLARE_CLASS(ClassName) \
    public: \
    ClassName (LPCTSTR name){LoadDll(name);} \
    ClassName () {PDLL();}

class PDLL
{
protected:
  HINSTANCE m_dllHandle;
private:
    LPTSTR m_dllName;
    int m_refCount;

public:

    PDLL()
    {
        m_dllHandle = NULL;
        m_dllName = NULL;
        m_refCount = 0;
    }

    //A NULL here means the name has already been set
    void LoadDll(LPCTSTR name, bool showMsg = true)
    {
        if (name)
            SetDllName(name);

        //try to load
        m_dllHandle = LoadLibrary(m_dllName);

        if (m_dllHandle == NULL && showMsg)
        {
          std::ostringstream message;
          message << "failed to load dll: " << ::GetLastError();
          throw std::runtime_error(message.str().c_str());
        }
    }

    bool SetDllName(LPCTSTR newName)
    {
        bool retVal = false;

        //we allow name resets only if the current DLL handle is invalid
        //once they've hooked into a DLL, the  name cannot be changed
        if (!m_dllHandle)
        {
            if (m_dllName)
            {
                delete []m_dllName;
                m_dllName = NULL;
            }

            //They may be setting this null (e.g., uninitialize)
            if (newName)
            {
                m_dllName = new TCHAR[_tcslen(newName) + 1];
                _tcscpy(m_dllName, newName);
            }
            retVal = true;
        }
        return retVal;
    }

    virtual bool Initialize(short showMsg = 1)
    {

        bool retVal = false;

        //Add one to our internal reference counter
        m_refCount++;

        if (m_refCount == 1 && m_dllName) //if this is first time, load the DLL
        {
            //we are assuming the name is already set
            LoadDll(NULL, showMsg);
            retVal = (m_dllHandle != NULL);
        }
        return retVal;
    }

    virtual void Uninitialize(void)
    {
        //If we're already completely unintialized, early exit
        if (!m_refCount)
            return;
        //if this is the last time this instance has been unitialized,
        //then do a full uninitialization
        m_refCount--;

        if (m_refCount < 1)
        {
            if (m_dllHandle)
            {
                FreeLibrary(m_dllHandle);
                m_dllHandle = NULL;
            }

            SetDllName(NULL); //clear out the name & free memory
        }
    }

    virtual ~PDLL()
    {
        //force this to be a true uninitialize
        m_refCount = 1;
        Uninitialize();

        //free name
        if (m_dllName)
        {
            delete [] m_dllName;
            m_dllName = NULL;
        }
    }

};
#endif
