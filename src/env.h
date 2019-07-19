namespace env
{

class Module;
class SecurityProduct;
class WindowsInfo;
class Metrics;

struct HandleCloser
{
  using pointer = HANDLE;

  void operator()(HANDLE h)
  {
    if (h != INVALID_HANDLE_VALUE) {
      ::CloseHandle(h);
    }
  }
};

using HandlePtr = std::unique_ptr<HANDLE, HandleCloser>;


struct LibraryFreer
{
  using pointer = HINSTANCE;

  void operator()(HINSTANCE h)
  {
    if (h != 0) {
      ::FreeLibrary(h);
    }
  }
};

struct COMReleaser
{
  void operator()(IUnknown* p)
  {
    if (p) {
      p->Release();
    }
  }
};


template <class T>
using COMPtr = std::unique_ptr<T, COMReleaser>;


class Console
{
public:
  Console();
  ~Console();

private:
  bool m_hasConsole;
  FILE* m_in;
  FILE* m_out;
  FILE* m_err;
};


// represents the process's environment
//
class Environment
{
public:
  Environment();
  ~Environment();

  // list of loaded modules in the current process
  //
  const std::vector<Module>& loadedModules() const;

  // information about the operating system
  //
  const WindowsInfo& windowsInfo() const;

  // information about the installed security products
  //
  const std::vector<SecurityProduct>& securityProducts() const;

  // information about displays
  //
  const Metrics& metrics() const;

  // logs the environment
  //
  void dump() const;

private:
  std::vector<Module> m_modules;
  std::unique_ptr<WindowsInfo> m_windows;
  std::vector<SecurityProduct> m_security;
  std::unique_ptr<Metrics> m_metrics;
};


enum class CoreDumpTypes
{
  Mini = 1,
  Data,
  Full
};

// creates a minidump file for the given process
//
bool coredump(CoreDumpTypes type);

// finds another process with the same name as this one and creates a minidump
// file for it
//
bool coredumpOther(CoreDumpTypes type);

} // namespace env
