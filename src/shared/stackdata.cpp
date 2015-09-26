#include "stackdata.h"

#include "util.h"
#include <DbgHelp.h>
#include <sstream>
#include <TlHelp32.h>
#include <set>
#include "error_report.h"
#include <boost/predef.h>


using namespace MOShared;

#if defined _MSC_VER

static void initDbgIfNecess()
{
  HANDLE process = ::GetCurrentProcess();
  static std::set<DWORD> initialized;
  if (initialized.find(::GetCurrentProcessId()) == initialized.end()) {
    static bool firstCall = true;
    if (firstCall) {
      ::SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
      firstCall = false;
    }
    if (!::SymInitialize(process, NULL, TRUE)) {
      printf("failed to initialize symbols: %d", ::GetLastError());
    }
    initialized.insert(::GetCurrentProcessId());
  }
}



StackData::StackData()
  : m_Count(0)
  , m_Function()
  , m_Line(-1)
{
  initTrace();
}

StackData::StackData(const char *function, int line)
  : m_Count(0)
  , m_Function(function)
  , m_Line(line)
{

}

std::string StackData::toString() const {
  initDbgIfNecess();

  char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
  PSYMBOL_INFO symbol = (PSYMBOL_INFO)buffer;
  symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
  symbol->MaxNameLen = MAX_SYM_NAME;

  std::ostringstream stackStream;

  if (m_Function.length() > 0) {
    stackStream << "[" << m_Function << ":" << m_Line << "]\n";
  }

  for(unsigned int i = 0; i < m_Count; ++i) {
    DWORD64 displacement = 0;
    if (!::SymFromAddr(::GetCurrentProcess(), (DWORD64)m_Stack[i], &displacement, symbol)) {
      stackStream << m_Count - i - 1 << ": [" << m_Stack[i] << "]\n";
    } else {
      stackStream << m_Count - i - 1 << ": " << symbol->Name << "\n";
    }
  }
  return stackStream.str();
}

void StackData::load_modules(HANDLE process, DWORD processID) {
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, processID);
  if (snap == INVALID_HANDLE_VALUE)
    return;

  MODULEENTRY32 entry;
  entry.dwSize = sizeof(entry);

  if (Module32First(snap, &entry)) {
    do {
      std::string fileName    = ToString(entry.szExePath, false);
      std::string moduleName  = ToString(entry.szModule, false);
      SymLoadModule64(process, NULL, fileName.c_str(), moduleName.c_str(), (DWORD64) entry.modBaseAddr, entry.modBaseSize);
    } while (Module32Next(snap, &entry));
  }
  CloseHandle(snap);
}

#pragma warning( disable : 4748 )

void StackData::initTrace() {
#ifdef _X86_
  load_modules(::GetCurrentProcess(), ::GetCurrentProcessId());
  CONTEXT context;
  std::memset(&context, 0, sizeof(CONTEXT));
  context.ContextFlags = CONTEXT_CONTROL;
#if BOOST_ARCH_X86_64
  ::RtlCaptureContext(&context);
#else
  __asm
  {
  Label:
    mov [context.Ebp], ebp;
    mov [context.Esp], esp;
    mov eax, [Label];
    mov [context.Eip], eax;
  }
#endif

  STACKFRAME64 stackFrame;
  ::ZeroMemory(&stackFrame, sizeof(STACKFRAME64));
  stackFrame.AddrPC.Offset    = context.Eip;
  stackFrame.AddrPC.Mode      = AddrModeFlat;
  stackFrame.AddrFrame.Offset = context.Ebp;
  stackFrame.AddrFrame.Mode   = AddrModeFlat;
  stackFrame.AddrStack.Offset = context.Esp;
  stackFrame.AddrStack.Mode   = AddrModeFlat;
  m_Count = 0;
  while (m_Count < FRAMES_TO_CAPTURE) {
    if (!StackWalk64(IMAGE_FILE_MACHINE_I386, ::GetCurrentProcess(),
                     ::GetCurrentThread(), &stackFrame, &context, NULL,
                     &SymFunctionTableAccess64, &SymGetModuleBase64, NULL)) {
      break;
    }

    if (stackFrame.AddrPC.Offset == 0) {
      continue;
      break;
    }

    m_Stack[m_Count++] = reinterpret_cast<void *>(stackFrame.AddrPC.Offset);
  }
#endif
}


bool MOShared::operator==(const StackData &LHS, const StackData &RHS) {
  if (LHS.m_Count != RHS.m_Count) {
    return false;
  } else {
    for (int i = 0; i < LHS.m_Count; ++i) {
      if (LHS.m_Stack[i] != RHS.m_Stack[i]) {
        return false;
      }
    }
  }
  return true;
}


bool MOShared::operator<(const StackData &LHS, const StackData &RHS) {
  if (LHS.m_Count != RHS.m_Count) {
    return LHS.m_Count < RHS.m_Count;
  } else {
    for (int i = 0; i < LHS.m_Count; ++i) {
      if (LHS.m_Stack[i] != RHS.m_Stack[i]) {
        return LHS.m_Stack[i] < RHS.m_Stack[i];
      }
    }
  }
  return false;
}
#endif
