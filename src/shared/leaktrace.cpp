#include "leaktrace.h"
#include "stackdata.h"
#include <Windows.h>
#include <set>
#include <map>
#include <vector>
#include <algorithm>


using namespace MOShared;


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
    for (auto& m_Trace : m_Traces)
    {
      result[m_Trace.second].push_back(m_Trace.first);
    }
    for (auto& iter : result)
    {
      printf("-----------------------------------\n"
             "%d objects not freed, allocated at:\n%s",
             static_cast<int>(iter.second.size()), iter.first.toString().c_str());
      printf("Addresses: ");
      for (int i = 0;
           i < (std::min<int>)(5, static_cast<int>(iter.second.size())); ++i) {
        printf("%p, ", reinterpret_cast<void *>(iter.second[i]));
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
