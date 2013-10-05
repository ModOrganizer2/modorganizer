#include "leaktrace.h"
#include <Windows.h>
#include <DbgHelp.h>
#include <set>
#include <map>
#include <vector>
#include <sstream>


static const int FRAMES_TO_SKIP = 3;     // StackData::StackData(), __TraceData::regTrace(), TraceAlloc()
static const int FRAMES_TO_CAPTURE = 10;


void initDbgIfNecessary()
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


class StackData {
  friend bool operator==(const StackData &LHS, const StackData &RHS);
  friend bool operator<(const StackData &LHS, const StackData &RHS);
public:

  StackData()
    : m_FunctionName("Dummy"), m_CodeLine(0)
  {}
  StackData(const char *functionName, int line) {
    m_Count = ::CaptureStackBackTrace(FRAMES_TO_SKIP, FRAMES_TO_CAPTURE, m_Stack, &m_Hash);
    m_FunctionName = functionName;
    m_CodeLine = line;
    if (m_Count == 0) {
      // TODO in this case the hash doesn't seem to be set. This is of course not a good solution
      m_Hash = reinterpret_cast<unsigned long>(m_FunctionName) + m_CodeLine;
    }
  }
  std::string toString() const {
    initDbgIfNecessary();

    char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
    PSYMBOL_INFO symbol = (PSYMBOL_INFO)buffer;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = MAX_SYM_NAME;

    std::ostringstream stackStream;

    stackStream << m_FunctionName << " [" << m_CodeLine << "]\n";

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
private:
  LPVOID m_Stack[FRAMES_TO_CAPTURE];
  USHORT m_Count;
  ULONG m_Hash;
  const char *m_FunctionName;
  int m_CodeLine;
};

bool operator==(const StackData &LHS, const StackData &RHS) {
  return LHS.m_Hash == RHS.m_Hash;
}

bool operator<(const StackData &LHS, const StackData &RHS) {
  return LHS.m_Hash < RHS.m_Hash;
}


static struct __TraceData {
  void regTrace(void *pointer, const char *functionName, int line) {
    m_Traces[reinterpret_cast<unsigned long>(pointer)] = StackData(functionName, line);
  }
  void deregTrace(void *pointer) {
    auto iter = m_Traces.find(reinterpret_cast<unsigned long>(pointer));
    if (iter != m_Traces.end()) {
      m_Traces.erase(iter);
    }
  }

  ~__TraceData() {
    std::map<StackData, std::vector<unsigned long> > result;
    for (auto iter = m_Traces.begin(); iter != m_Traces.end(); ++iter) {
      result[iter->second].push_back(iter->first);
    }
    for (auto iter = result.begin(); iter != result.end(); ++iter) {
      printf("-----------------------------------\n"
             "%d objects not freed, allocated at:\n%s",
             iter->second.size(), iter->first.toString().c_str());
      printf("Addresses: ");
      for (int i = 0; i < (std::min<int>)(5, iter->second.size()); ++i) {
        printf("%p, ", iter->second[i]);
      }
      printf("\n");
    }
  }

  std::map<unsigned long, StackData> m_Traces;

} __trace;


void LeakTrace::TraceAlloc(void *ptr, const char *functionName, int line)
{
  __trace.regTrace(ptr, functionName, line);
}

void LeakTrace::TraceDealloc(void *ptr)
{
  __trace.deregTrace(ptr);
}
