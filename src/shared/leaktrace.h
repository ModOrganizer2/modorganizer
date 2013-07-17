#ifndef LEAKTRACE_H
#define LEAKTRACE_H


namespace LeakTrace {

void TraceAlloc(void *ptr);
void TraceDealloc(void *ptr);

};

#ifdef TRACE_LEAKS

#define LEAK_TRACE LeakTrace::TraceAlloc(this)
#define LEAK_UNTRACE LeakTrace::TraceDealloc(this)

#else // TRACE_LEAKS

#define LEAK_TRACE
#define LEAK_UNTRACE

#endif // TRACE_LEAKS

#endif // LEAKTRACE_H
