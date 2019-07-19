namespace env
{

class Module;
class SecurityProduct;
class WindowsInfo;
class Metrics;


// used by HandlePtr, calls CloseHandle() as the deleter
//
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


// used by DesktopDCPtr, calls ReleaseDC(0, dc) as the deleter
//
struct DesktopDCReleaser
{
  using pointer = HDC;

  void operator()(HDC dc)
  {
    if (dc != 0) {
      ::ReleaseDC(0, dc);
    }
  }
};

using DesktopDCPtr = std::unique_ptr<HDC, DesktopDCReleaser>;


// used by LibraryPtr, calls FreeLibrary as the deleter
//
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

using LibraryPtr = std::unique_ptr<HINSTANCE, LibraryFreer>;


// used by COMPtr, calls Release() as the deleter
//
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


// creates a console in the constructor and destroys it in the destructor,
// also redirects standard streams
//
class Console
{
public:
  // opens the console and redirects standard streams to it
  //
  Console();

  // destroys the console and redirects the standard stream to NUL
  //
  ~Console();

private:
  // whether the console was allocated successfully
  bool m_hasConsole;

  // standard streams
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
